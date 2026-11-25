import {
  Component,
  createContext,
  type ReactNode,
  useContext,
  useEffect,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { useLocation, useMatch } from "react-router-dom";
import {
  isHubIdentifier,
  parseHubPageDescriptor,
} from "../../../../modules/modules/feature-host/HubPageDescriptorValidation.ts";
import styles from "./feature-hub-provider.module.css";
import type {
  ActiveHubRequest,
  HubAcceptedSnapshot,
  HubEntry,
  HubEnvelope,
  HubNavigationCandidate,
  HubNavigationItem,
  HubProviderProps,
  HubScalar,
  HubSnapshot,
  HubState,
  HubTranslation,
  HubTranslationParams,
  HubTranslationResult,
  HubViewBoundaryProps,
  HubViewErrorBoundaryProps,
  HubViewErrorBoundaryState,
} from "./types.ts";

const EMPTY_SNAPSHOT: HubSnapshot = { epoch: "", hubRevision: 0, entries: [] };
const HubContext = createContext<HubState | null>(null);
const MAX_SNAPSHOT_BYTES = 131_584;
const MAX_ENTRIES = 32;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null &&
    Object.getPrototypeOf(value) === Object.prototype;
}

function hasOnlyKeys(
  value: Record<string, unknown>,
  allowed: readonly string[],
): boolean {
  return Object.keys(value).every((key) => allowed.includes(key));
}

function isScalarRecord(value: unknown): value is Record<string, HubScalar> {
  return isRecord(value) && Object.keys(value).length <= 8 &&
    Object.entries(value).every(([key, item]) =>
      /^[A-Za-z][A-Za-z0-9_]{0,31}$/.test(key) &&
      !["__proto__", "constructor", "prototype"].includes(key) &&
      (typeof item === "string" && item.length <= 128 ||
        typeof item === "number" && Number.isFinite(item) &&
          Math.abs(item) <= 1_000_000_000 ||
        typeof item === "boolean")
    );
}

function parseSnapshot(
  json: string,
): HubEnvelope | null {
  if (new TextEncoder().encode(json).byteLength > MAX_SNAPSHOT_BYTES) {
    return null;
  }
  try {
    const envelope: unknown = JSON.parse(json);
    if (
      !isRecord(envelope) || envelope.type !== "snapshot" ||
      !hasOnlyKeys(envelope, [
        "type",
        "connectionNonce",
        "requestId",
        "snapshot",
      ]) ||
      typeof envelope.connectionNonce !== "string" ||
      envelope.connectionNonce.length < 1 ||
      envelope.connectionNonce.length > 80 ||
      typeof envelope.requestId !== "string" ||
      envelope.requestId.length < 1 || envelope.requestId.length > 80 ||
      !isRecord(envelope.snapshot)
    ) return null;
    const raw = envelope.snapshot;
    if (
      !hasOnlyKeys(raw, ["epoch", "hubRevision", "entries"]) ||
      typeof raw.epoch !== "string" || raw.epoch.length < 1 ||
      raw.epoch.length > 128 ||
      !Number.isSafeInteger(raw.hubRevision) ||
      (raw.hubRevision as number) < 0 ||
      !Array.isArray(raw.entries) ||
      raw.entries.length > MAX_ENTRIES
    ) return null;
    const entries: HubEntry[] = [];
    const routes = new Set<string>();
    const ownerCounts = new Map<string, number>();
    for (const item of raw.entries) {
      if (
        !isRecord(item) || typeof item.featureId !== "string" ||
        !hasOnlyKeys(item, [
          "featureId",
          "ownerHostEpoch",
          "ownerGeneration",
          "localId",
          "route",
          "titleKey",
          "navigation",
          "view",
        ]) ||
        !isHubIdentifier(item.featureId) ||
        typeof item.localId !== "string" || !isHubIdentifier(item.localId) ||
        typeof item.ownerHostEpoch !== "string" ||
        item.ownerHostEpoch.length < 1 || item.ownerHostEpoch.length > 128 ||
        !Number.isSafeInteger(item.ownerGeneration) ||
        (item.ownerGeneration as number) < 1 ||
        !isRecord(item.view) ||
        !hasOnlyKeys(item.view, ["kind", "titleKey", "bodyKey", "params"]) ||
        item.view.kind !== "message"
      ) return null;
      if (
        item.navigation !== undefined && (!isRecord(item.navigation) ||
          !hasOnlyKeys(item.navigation, ["labelKey", "order"]) ||
          item.navigation.labelKey === undefined)
      ) return null;
      const params = item.view.params;
      if (params !== undefined && !isScalarRecord(params)) return null;
      let descriptor;
      try {
        descriptor = parseHubPageDescriptor(
          JSON.stringify({
            localId: item.localId,
            route: item.route,
            titleKey: item.titleKey,
            ...(item.navigation === undefined
              ? {}
              : { navigation: item.navigation }),
            view: item.view,
          }),
          item.featureId,
        );
      } catch {
        return null;
      }
      if (routes.has(descriptor.route)) return null;
      routes.add(descriptor.route);
      const ownerKey = JSON.stringify([
        item.featureId,
        item.ownerHostEpoch,
        item.ownerGeneration,
      ]);
      const ownerCount = (ownerCounts.get(ownerKey) ?? 0) + 1;
      if (ownerCount > 16) return null;
      ownerCounts.set(ownerKey, ownerCount);
      entries.push({
        ...descriptor,
        featureId: item.featureId,
        ownerHostEpoch: item.ownerHostEpoch,
        ownerGeneration: item.ownerGeneration as number,
      });
    }
    return {
      requestId: envelope.requestId,
      connectionNonce: envelope.connectionNonce,
      snapshot: {
        epoch: raw.epoch,
        hubRevision: raw.hubRevision as number,
        entries,
      },
    };
  } catch {
    return null;
  }
}

export function FeatureHubProvider({ children }: HubProviderProps) {
  const location = useLocation();
  const dynamicRouteMatch = useMatch("/extensions/:featureId/:localId");
  const { t, i18n } = useTranslation();
  const translateSafely: HubTranslation = (
    key: string,
    params: HubTranslationParams = {},
  ): HubTranslationResult => {
    const languages = [
      ...new Set([
        i18n.resolvedLanguage,
        ...i18n.languages,
        i18n.language,
      ].filter((language): language is string => typeof language === "string")),
    ];
    const resolve = (
      resourceKey: string,
      interpolation: HubTranslationParams = {},
    ): string | undefined => {
      for (const language of languages) {
        let resource: unknown;
        try {
          resource = i18n.getResource(
            language,
            "translations",
            resourceKey,
          );
        } catch {
          return undefined;
        }
        if (resource === undefined) continue;
        if (typeof resource !== "string" || resource.length === 0) {
          return undefined;
        }
        try {
          const translated: unknown = t(resourceKey, {
            ns: "translations",
            nsSeparator: false,
            lng: language,
            returnObjects: false,
            defaultValue: "",
            replace: interpolation,
          });
          if (
            typeof translated === "string" && translated.length > 0 &&
            translated !== resourceKey
          ) {
            return translated;
          }
        } catch {
          return undefined;
        }
        return undefined;
      }
      return undefined;
    };
    const translated = resolve(key, params);
    if (translated !== undefined) return { value: translated, found: true };
    const fallback = key === "featureHub.safeUnavailable"
      ? undefined
      : resolve("featureHub.safeUnavailable");
    return {
      value: fallback ?? "Feature content is unavailable.",
      found: false,
    };
  };
  const [status, setStatus] = useState<HubState["status"]>("connecting");
  const [requestId, setRequestId] = useState<string | null>(null);
  const [connectionNonce, setConnectionNonce] = useState<string | null>(null);
  const [snapshot, setSnapshot] = useState(EMPTY_SNAPSHOT);
  const snapshotRef = useRef(snapshot);
  const [renderedCommit, markRendered] = useState<string | null>(null);
  const activeRequest = useRef<ActiveHubRequest | null>(null);
  const acceptedSnapshot = useRef<HubAcceptedSnapshot | null>(null);

  useEffect(() => {
    const beginHandshake = () => {
      const nextRequestId = globalThis.crypto.randomUUID();
      activeRequest.current = {
        requestId: nextRequestId,
        connectionNonce: null,
      };
      acceptedSnapshot.current = null;
      setRequestId(nextRequestId);
      setConnectionNonce(null);
      setStatus("resyncing");
      markRendered(null);
      globalThis.dispatchEvent(
        new CustomEvent("nora:feature-hub-connect", {
          detail: nextRequestId,
        }),
      );
    };
    const onSnapshot = (event: Event) => {
      const detail = (event as CustomEvent<unknown>).detail;
      if (typeof detail !== "string") return;
      const parsed = parseSnapshot(detail);
      const active = activeRequest.current;
      if (!parsed || !active || parsed.requestId !== active.requestId) return;
      const previous = acceptedSnapshot.current;
      if (
        previous && (parsed.requestId !== previous.requestId ||
          parsed.connectionNonce !== previous.connectionNonce ||
          parsed.snapshot.epoch !== previous.epoch ||
          parsed.snapshot.hubRevision < previous.revision)
      ) return;
      if (previous && parsed.snapshot.hubRevision === previous.revision) {
        if (
          JSON.stringify(snapshotRef.current) !==
            JSON.stringify(parsed.snapshot)
        ) return;
        return;
      }
      active.connectionNonce = parsed.connectionNonce;
      acceptedSnapshot.current = {
        requestId: parsed.requestId,
        connectionNonce: parsed.connectionNonce,
        epoch: parsed.snapshot.epoch,
        revision: parsed.snapshot.hubRevision,
      };
      setStatus("ready");
      setRequestId(parsed.requestId);
      setConnectionNonce(parsed.connectionNonce);
      setSnapshot(parsed.snapshot);
      snapshotRef.current = parsed.snapshot;
      markRendered(null);
    };
    const onDisconnect = () => {
      activeRequest.current = null;
      acceptedSnapshot.current = null;
      setRequestId(null);
      setConnectionNonce(null);
      setStatus("disconnected");
      markRendered(null);
    };
    const onReady = () => beginHandshake();
    globalThis.addEventListener("nora:feature-hub-snapshot", onSnapshot);
    globalThis.addEventListener("nora:feature-hub-disconnected", onDisconnect);
    globalThis.addEventListener("nora:feature-hub-ready", onReady);
    globalThis.addEventListener("nora:feature-hub-resyncing", onReady);
    beginHandshake();
    return () => {
      const active = activeRequest.current;
      if (active) {
        globalThis.dispatchEvent(
          new CustomEvent("nora:feature-hub-disconnect", {
            detail: JSON.stringify({
              requestId: active.requestId,
              connectionNonce: active.connectionNonce,
            }),
          }),
        );
      }
      globalThis.removeEventListener("nora:feature-hub-snapshot", onSnapshot);
      globalThis.removeEventListener(
        "nora:feature-hub-disconnected",
        onDisconnect,
      );
      globalThis.removeEventListener("nora:feature-hub-ready", onReady);
      globalThis.removeEventListener("nora:feature-hub-resyncing", onReady);
    };
  }, []);

  const dynamicPath = dynamicRouteMatch !== null;
  const currentEntry = snapshot.entries.find((entry) =>
    entry.route === location.pathname
  );
  const navigationReady = snapshot.entries.every((entry) => {
    if (!entry.navigation) return true;
    return translateSafely(entry.navigation.labelKey).found;
  });
  const currentCommit = dynamicPath && requestId && connectionNonce
    ? [
      requestId,
      connectionNonce,
      snapshot.epoch,
      snapshot.hubRevision,
      location.pathname,
      currentEntry
        ? `${currentEntry.ownerHostEpoch}:${currentEntry.ownerGeneration}`
        : "absent",
    ].join(":")
    : null;

  useEffect(() => {
    if (status !== "ready" || !connectionNonce || !navigationReady) return;
    if (dynamicPath && (!currentCommit || renderedCommit !== currentCommit)) {
      return;
    }
    const entry = JSON.stringify({
      type: "snapshot-ack",
      requestId,
      connectionNonce,
      epoch: snapshot.epoch,
      hubRevision: snapshot.hubRevision,
    });
    globalThis.dispatchEvent(
      new CustomEvent("nora:feature-hub-ack", { detail: entry }),
    );
  }, [
    connectionNonce,
    currentCommit,
    currentEntry,
    dynamicPath,
    navigationReady,
    requestId,
    renderedCommit,
    snapshot,
    status,
  ]);

  const value = useMemo<HubState>(() => ({
    status,
    requestId,
    connectionNonce,
    snapshot,
    renderedCommit,
    markRendered,
    translate: translateSafely,
  }), [
    connectionNonce,
    i18n.language,
    renderedCommit,
    requestId,
    snapshot,
    status,
    t,
    translateSafely,
  ]);

  return <HubContext.Provider value={value}>{children}</HubContext.Provider>;
}

export function useFeatureHub(): HubState {
  const value = useContext(HubContext);
  if (!value) throw new Error("FeatureHubProvider is missing");
  return value;
}

class HubViewErrorBoundary extends Component<
  HubViewErrorBoundaryProps,
  HubViewErrorBoundaryState
> {
  override state: HubViewErrorBoundaryState = { failed: false };

  static getDerivedStateFromError(): HubViewErrorBoundaryState {
    return { failed: true };
  }

  override componentDidCatch(error: Error): void {
    console.error("[FeatureHub] Dynamic page render failed", error);
  }

  override render(): ReactNode {
    if (this.state.failed) {
      return (
        <section className={styles.section} role="alert">
          {this.props.fallback}
        </section>
      );
    }
    return this.props.children;
  }
}

export function FeatureHubViewBoundary({
  children,
}: HubViewBoundaryProps) {
  const location = useLocation();
  const { connectionNonce, requestId, snapshot, translate } = useFeatureHub();
  const fallback = translate("featureHub.safeUnavailable").value;
  const entry = snapshot.entries.find((candidate) =>
    candidate.route === location.pathname
  );
  const boundaryKey = [
    requestId,
    connectionNonce,
    snapshot.epoch,
    snapshot.hubRevision,
    location.pathname,
    entry ? `${entry.ownerHostEpoch}:${entry.ownerGeneration}` : "absent",
  ].join(":");
  return (
    <HubViewErrorBoundary key={boundaryKey} fallback={fallback}>
      {children}
    </HubViewErrorBoundary>
  );
}

export function HubExtensionPage() {
  const location = useLocation();
  const {
    connectionNonce,
    requestId,
    snapshot,
    translate,
    markRendered,
  } = useFeatureHub();
  const entry = snapshot.entries.find((candidate) =>
    candidate.route === location.pathname
  );
  const commit = requestId && connectionNonce
    ? [
      requestId,
      connectionNonce,
      snapshot.epoch,
      snapshot.hubRevision,
      location.pathname,
      entry ? `${entry.ownerHostEpoch}:${entry.ownerGeneration}` : "absent",
    ].join(":")
    : null;
  const title = entry
    ? translate(entry.view.titleKey, entry.view.params)
    : null;
  const body = entry ? translate(entry.view.bodyKey, entry.view.params) : null;

  const unavailable = translate("featureHub.unavailableTitle");
  const detail = translate("featureHub.unavailableBody");
  const unavailableCommitted = !entry && unavailable.found && detail.found;
  const entryCommitted = Boolean(entry && title?.found && body?.found);

  useLayoutEffect(() => {
    markRendered(
      commit && (entryCommitted || unavailableCommitted) ? commit : null,
    );
  }, [
    body?.found,
    commit,
    entryCommitted,
    markRendered,
    title?.found,
    unavailableCommitted,
  ]);

  if (!entry || !title?.found || !body?.found) {
    return (
      <section
        className={styles.section}
        key={commit ?? "unavailable-pending"}
        aria-labelledby="hub-unavailable-title"
      >
        <h1 id="hub-unavailable-title">{unavailable.value}</h1>
        <p>{detail.value}</p>
      </section>
    );
  }

  return (
    <section
      className={styles.section}
      key={commit ?? "message-pending"}
      aria-labelledby="hub-message-title"
    >
      <h1 id="hub-message-title">{title.value}</h1>
      <p>{body.value}</p>
    </section>
  );
}

export function featureHubNavigation(
  snapshot: HubSnapshot,
  translate: HubTranslation,
): HubNavigationItem[] {
  const candidates: HubNavigationCandidate[] = snapshot.entries.flatMap(
    (entry) => {
      if (!entry.navigation) return [];
      const label = translate(entry.navigation.labelKey);
      return [{
        title: label.value,
        url: entry.route,
        key:
          `${entry.ownerHostEpoch}:${entry.ownerGeneration}:${entry.featureId}:${entry.localId}`,
        order: entry.navigation.order ?? 0,
      }];
    },
  );
  return candidates.sort((left, right) =>
    left.order - right.order ||
    (left.url < right.url ? -1 : left.url > right.url ? 1 : 0)
  )
    .map(({ title, url, key }) => ({ title, url, key }));
}
