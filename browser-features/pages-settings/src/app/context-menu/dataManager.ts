/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useCallback, useEffect, useRef, useState } from "react";
import { rpc } from "@/lib/rpc/rpc.ts";
import type {
  ContextMenuCatalogSnapshot,
  ContextMenuConfig,
  ContextMenuItemDescriptor,
} from "#features-chrome/common/context-menu/types.ts";
import {
  CONTEXT_MENU_CONFIG_PREF,
  CONTEXT_MENU_ENABLED_PREF,
} from "#features-chrome/common/context-menu/types.ts";
import {
  parseContextMenuConfigWithStatus,
  serializeContextMenuConfig,
} from "#features-chrome/common/context-menu/config.ts";
import {
  type ContextMenuPersistence,
  type ContextMenuPersistenceError,
  type ContextMenuPersistenceSnapshot,
  type ContextMenuPersistenceVersions,
  createContextMenuPersistence,
} from "./configPersistence.ts";
import {
  type ContextMenuLevelTarget,
  createDefaultContextMenuConfig,
  moveContextMenuItemBeforeKey,
  reorderContextMenuItemByKey,
  resetContextMenuProfile,
  setContextMenuItemHidden,
  setContextMenuProfileIndependent,
} from "./operations.ts";

export interface ContextMenuSettingsModel {
  enabled: boolean;
  config: ContextMenuConfig;
  catalog: ContextMenuCatalogSnapshot | null;
  loading: boolean;
  catalogLoading: boolean;
  pending: boolean;
  saveError: ContextMenuPersistenceError | null;
  enabledUnavailable: boolean;
  loadError: boolean;
  loadErrorKind:
    | "read"
    | "invalid"
    | "unsupported-version"
    | "conflict"
    | null;
  catalogError: boolean;
  toggleEnabled(): Promise<boolean>;
  setItemVisible(
    target: ContextMenuLevelTarget,
    itemKey: string,
    visible: boolean,
  ): Promise<boolean>;
  moveItem(
    target: ContextMenuLevelTarget,
    catalogItems: readonly ContextMenuItemDescriptor[],
    activeKey: string,
    overKey: string,
  ): Promise<boolean>;
  moveItemBefore(
    target: ContextMenuLevelTarget,
    catalogItems: readonly ContextMenuItemDescriptor[],
    activeKey: string,
    beforeKey?: string | null,
  ): Promise<boolean>;
  setProfileIndependent(
    surfaceKey: string,
    profileKey: string,
    independent: boolean,
  ): Promise<boolean>;
  resetProfile(surfaceKey: string, profileKey: string): Promise<boolean>;
  resetAll(): Promise<boolean>;
  repairInvalidConfig(): Promise<boolean>;
  reloadCatalog(): Promise<void>;
}

const DEFAULT_PERSISTENCE_SNAPSHOT: ContextMenuPersistenceSnapshot = {
  committed: {
    enabled: true,
    config: createDefaultContextMenuConfig(),
  },
  projected: {
    enabled: true,
    config: createDefaultContextMenuConfig(),
  },
  pending: false,
  error: null,
  blockingErrors: {
    enabled: null,
    config: null,
  },
};

function persistenceVersionsEqual(
  first: ContextMenuPersistenceVersions,
  second: ContextMenuPersistenceVersions,
): boolean {
  return first.enabled === second.enabled && first.config === second.config;
}

function catalogHasInitialWebPageItems(
  catalog: ContextMenuCatalogSnapshot,
): boolean {
  return (catalog.surfaces.find((surface) => surface.key === "browser.content")
    ?.profiles.find((profile) => profile.key === "page")?.containers.find(
      (container) => container.key === "root",
    )?.items.length ?? 0) > 0;
}

export function useContextMenuSettings(): ContextMenuSettingsModel {
  const [snapshot, setSnapshot] = useState<ContextMenuPersistenceSnapshot>(
    DEFAULT_PERSISTENCE_SNAPSHOT,
  );
  const [catalog, setCatalog] = useState<ContextMenuCatalogSnapshot | null>(
    null,
  );
  const [loading, setLoading] = useState(true);
  const [catalogLoading, setCatalogLoading] = useState(true);
  const [enabledUnavailable, setEnabledUnavailable] = useState(false);
  const [loadErrorKind, setLoadErrorKind] = useState<
    ContextMenuSettingsModel["loadErrorKind"]
  >(null);
  const loadErrorKindRef = useRef<ContextMenuSettingsModel["loadErrorKind"]>(
    null,
  );
  const [catalogError, setCatalogError] = useState(false);
  const persistenceRef = useRef<ContextMenuPersistence | null>(null);
  const unsafeConfigVersionRef = useRef<string | null>(null);
  const catalogMountedRef = useRef(false);
  const catalogRefreshRef = useRef<Promise<void> | null>(null);
  const catalogRefreshQueuedRef = useRef(false);
  const catalogRevisionRef = useRef(-1);
  const catalogHasInitialWebPageItemsRef = useRef(false);
  const configBlockingError = snapshot.blockingErrors.config;
  const enabledBlockingError = snapshot.blockingErrors.enabled;
  const unsafePersistenceError = configBlockingError?.kind === "unsafe-config"
    ? configBlockingError
    : null;
  const configPreferenceTypeError = configBlockingError?.kind ===
      "preference-type-mismatch"
    ? configBlockingError
    : null;
  const enabledPreferenceTypeError = enabledBlockingError?.kind ===
      "preference-type-mismatch"
    ? enabledBlockingError
    : null;
  const effectiveLoadErrorKind = loadErrorKind ??
    unsafePersistenceError?.status ??
    (configPreferenceTypeError || enabledPreferenceTypeError ? "read" : null);
  const effectiveEnabledUnavailable = enabledUnavailable ||
    enabledPreferenceTypeError?.preference === "enabled";

  const refreshCatalog = useCallback((showLoading: boolean): Promise<void> => {
    if (showLoading && catalogMountedRef.current) setCatalogLoading(true);
    const inFlight = catalogRefreshRef.current;
    if (inFlight) {
      // One extra pass is enough to observe every report that landed while the
      // current snapshot RPC was in progress. Keep returning the shared promise
      // so manual refresh waits for the queued pass as well.
      catalogRefreshQueuedRef.current = true;
      return inFlight;
    }

    const request = Promise.resolve().then(async () => {
      do {
        catalogRefreshQueuedRef.current = false;
        try {
          const nextCatalog = await rpc.getContextMenuCatalog();
          if (!catalogMountedRef.current) return;
          if (nextCatalog.revision < catalogRevisionRef.current) continue;
          catalogRevisionRef.current = nextCatalog.revision;
          catalogHasInitialWebPageItemsRef.current =
            catalogHasInitialWebPageItems(nextCatalog);
          setCatalog(nextCatalog);
          setCatalogError(false);
        } catch (error) {
          if (!catalogMountedRef.current) return;
          console.error(
            "[ContextMenuSettings] Failed to load the catalog",
            error,
          );
          setCatalogError(true);
        }
      } while (
        catalogMountedRef.current && catalogRefreshQueuedRef.current
      );
    });
    catalogRefreshRef.current = request;
    void request.finally(() => {
      if (catalogRefreshRef.current === request) {
        catalogRefreshRef.current = null;
        if (catalogMountedRef.current) setCatalogLoading(false);
      }
    });
    return request;
  }, []);

  useEffect(() => {
    let cancelled = false;
    let unsubscribe: (() => void) | null = null;
    let catalogRetryTimer: number | undefined;
    let catalogRevisionPollInFlight = false;
    let preferencesLoaded = false;
    let refreshInFlight = false;
    const loadPreferences = async (initial: boolean) => {
      if (refreshInFlight) return;
      const baselinePersistence = persistenceRef.current;
      const baselineVersions = baselinePersistence?.getVersions();
      if (!initial && baselinePersistence?.getSnapshot().pending) return;
      refreshInFlight = true;
      const [enabledResult, configResult] = await Promise.allSettled([
        rpc.getBoolPrefState(CONTEXT_MENU_ENABLED_PREF),
        rpc.getStringPrefState(CONTEXT_MENU_CONFIG_PREF),
      ]);
      refreshInFlight = false;
      if (cancelled) return;

      const enabled = enabledResult.status === "fulfilled"
        ? enabledResult.value.value ?? true
        : true;
      const enabledVersion = enabledResult.status === "fulfilled"
        ? enabledResult.value.value
        : null;
      const serializedConfig = configResult.status === "fulfilled"
        ? configResult.value.value
        : null;
      const parsedConfig = parseContextMenuConfigWithStatus(
        serializedConfig,
      );
      const config = parsedConfig.config;
      const enabledReadFailed = enabledResult.status === "rejected" ||
        enabledResult.value.typeMismatch;
      const configReadFailed = configResult.status === "rejected" ||
        configResult.value.typeMismatch;
      const preferenceReadFailed = enabledReadFailed || configReadFailed;
      const unsafeConfig = parsedConfig.status === "invalid" ||
        parsedConfig.status === "unsupported-version";
      const nextLoadErrorKind = preferenceReadFailed
        ? "read"
        : parsedConfig.status === "invalid"
        ? "invalid"
        : parsedConfig.status === "unsupported-version"
        ? "unsupported-version"
        : null;

      if (!initial && baselinePersistence) {
        const currentPersistenceSnapshot = baselinePersistence.getSnapshot();
        if (
          baselinePersistence !== persistenceRef.current ||
          currentPersistenceSnapshot.pending ||
          !baselineVersions ||
          !persistenceVersionsEqual(
            baselineVersions,
            baselinePersistence.getVersions(),
          )
        ) {
          return;
        }
        const nextVersions = {
          enabled: enabledVersion,
          config: serializedConfig,
        };
        if (
          enabledResult.status === "fulfilled" &&
          configResult.status === "fulfilled" &&
          nextLoadErrorKind === loadErrorKindRef.current &&
          currentPersistenceSnapshot.blockingErrors.enabled === null &&
          currentPersistenceSnapshot.blockingErrors.config === null &&
          persistenceVersionsEqual(baselineVersions, nextVersions)
        ) {
          return;
        }
      }

      unsubscribe?.();
      unsubscribe = null;
      persistenceRef.current = null;
      unsafeConfigVersionRef.current = unsafeConfig ? serializedConfig : null;
      setEnabledUnavailable(enabledReadFailed);
      setLoadErrorKind(nextLoadErrorKind);
      loadErrorKindRef.current = nextLoadErrorKind;

      if (enabledReadFailed) {
        // Never allow a fallback/default value to overwrite a preference that
        // merely failed to load. A later page reload can retry the read.
        setSnapshot({
          committed: { enabled, config },
          projected: { enabled, config },
          pending: false,
          error: null,
          blockingErrors: {
            enabled: null,
            config: null,
          },
        });
      } else {
        const persistence = createContextMenuPersistence(
          { enabled, config },
          {
            compareAndSetEnabled: (expectedValue, nextEnabled) =>
              rpc.compareAndSetBoolPref(
                CONTEXT_MENU_ENABLED_PREF,
                expectedValue,
                nextEnabled,
              ),
            compareAndSetConfig: (expectedValue, nextConfig) =>
              rpc.compareAndSetStringPref(
                CONTEXT_MENU_CONFIG_PREF,
                expectedValue,
                nextConfig,
              ),
          },
          { enabled: enabledVersion, config: serializedConfig },
        );
        persistenceRef.current = persistence;
        setSnapshot(persistence.getSnapshot());
        unsubscribe = persistence.subscribe(setSnapshot);
      }
      preferencesLoaded = true;
      if (initial) setLoading(false);
    };

    // Preference and catalog RPCs are independent: a menu that has not been
    // discovered yet must not delay the global enable/reset controls.
    const refreshFocusedData = () => {
      if (preferencesLoaded) void loadPreferences(false);
      void refreshCatalog(false);
    };
    const refreshVisibleData = () => {
      if (globalThis.document.visibilityState === "visible") {
        refreshFocusedData();
      }
    };
    catalogMountedRef.current = true;
    globalThis.addEventListener("focus", refreshFocusedData);
    globalThis.document.addEventListener(
      "visibilitychange",
      refreshVisibleData,
    );
    const catalogRevisionPoll = globalThis.setInterval(() => {
      if (
        cancelled || catalogRevisionPollInFlight ||
        globalThis.document.visibilityState !== "visible"
      ) return;
      catalogRevisionPollInFlight = true;
      void (async () => {
        try {
          const revision = await rpc.getContextMenuCatalogRevision();
          if (!cancelled && revision > catalogRevisionRef.current) {
            await refreshCatalog(false);
          }
        } catch {
          // Focus/visibility refresh and the manual button remain available if
          // a mixed-version or temporarily unavailable actor cannot probe.
        } finally {
          catalogRevisionPollInFlight = false;
        }
      })();
    }, 1_500);
    const retryDelays = [250, 1_000, 3_000] as const;
    const loadCatalogUntilPopulated = async (attempt = 0): Promise<void> => {
      await refreshCatalog(attempt === 0);
      if (
        cancelled || catalogHasInitialWebPageItemsRef.current ||
        attempt >= retryDelays.length
      ) return;

      // Schedule relative to the completed request. Fixed startup timers can
      // all coalesce into one slow in-flight RPC and leave an empty snapshot
      // with no retry remaining.
      catalogRetryTimer = globalThis.setTimeout(() => {
        void loadCatalogUntilPopulated(attempt + 1);
      }, retryDelays[attempt]);
    };
    void Promise.all([loadPreferences(true), loadCatalogUntilPopulated()]);
    return () => {
      cancelled = true;
      catalogMountedRef.current = false;
      catalogRefreshQueuedRef.current = false;
      globalThis.removeEventListener("focus", refreshFocusedData);
      globalThis.document.removeEventListener(
        "visibilitychange",
        refreshVisibleData,
      );
      globalThis.clearInterval(catalogRevisionPoll);
      if (catalogRetryTimer !== undefined) {
        globalThis.clearTimeout(catalogRetryTimer);
      }
      unsubscribe?.();
      persistenceRef.current = null;
    };
  }, [refreshCatalog]);

  const updateConfig = useCallback(
    (update: (current: Readonly<ContextMenuConfig>) => ContextMenuConfig) =>
      persistenceRef.current?.updateConfig(update) ?? Promise.resolve(false),
    [],
  );

  const toggleEnabled = useCallback(
    () =>
      persistenceRef.current?.updateEnabled((enabled) => !enabled) ??
        Promise.resolve(false),
    [],
  );

  const setItemVisible = useCallback(
    (
      target: ContextMenuLevelTarget,
      itemKey: string,
      visible: boolean,
    ) =>
      updateConfig((current) =>
        setContextMenuItemHidden(current, target, itemKey, !visible)
      ),
    [updateConfig],
  );

  const moveItem = useCallback(
    (
      target: ContextMenuLevelTarget,
      catalogItems: readonly ContextMenuItemDescriptor[],
      activeKey: string,
      overKey: string,
    ) =>
      updateConfig((current) =>
        reorderContextMenuItemByKey(
          current,
          target,
          catalogItems,
          activeKey,
          overKey,
        )
      ),
    [updateConfig],
  );

  const moveItemBefore = useCallback(
    (
      target: ContextMenuLevelTarget,
      catalogItems: readonly ContextMenuItemDescriptor[],
      activeKey: string,
      beforeKey?: string | null,
    ) =>
      updateConfig((current) =>
        moveContextMenuItemBeforeKey(
          current,
          target,
          catalogItems,
          activeKey,
          beforeKey,
        )
      ),
    [updateConfig],
  );

  const setProfileIndependent = useCallback(
    (surfaceKey: string, profileKey: string, independent: boolean) =>
      updateConfig((current) =>
        setContextMenuProfileIndependent(
          current,
          surfaceKey,
          profileKey,
          independent,
        )
      ),
    [updateConfig],
  );

  const resetProfile = useCallback(
    (surfaceKey: string, profileKey: string) =>
      updateConfig((current) =>
        resetContextMenuProfile(current, surfaceKey, profileKey)
      ),
    [updateConfig],
  );

  const resetAll = useCallback(
    () => updateConfig(() => createDefaultContextMenuConfig()),
    [updateConfig],
  );

  const repairInvalidConfig = useCallback(async () => {
    if (effectiveLoadErrorKind !== "invalid") return false;
    const serializedDefault = serializeContextMenuConfig(
      createDefaultContextMenuConfig(),
    );
    const expectedValue = unsafePersistenceError?.status === "invalid"
      ? unsafePersistenceError.currentValue
      : unsafeConfigVersionRef.current;
    try {
      const result = await rpc.compareAndSetStringPref(
        CONTEXT_MENU_CONFIG_PREF,
        expectedValue,
        serializedDefault,
      );
      if (result.typeMismatch) {
        loadErrorKindRef.current = "read";
        setLoadErrorKind("read");
        return false;
      }
      if (result.updated || result.currentValue === serializedDefault) {
        globalThis.location.reload();
        return true;
      }
      unsafeConfigVersionRef.current = result.currentValue;
      loadErrorKindRef.current = "conflict";
      setLoadErrorKind("conflict");
      return false;
    } catch (error) {
      console.error(
        "[ContextMenuSettings] Failed to repair the configuration",
        error,
      );
      loadErrorKindRef.current = "read";
      setLoadErrorKind("read");
      return false;
    }
  }, [effectiveLoadErrorKind, unsafePersistenceError]);

  const reloadCatalog = useCallback(async () => {
    await refreshCatalog(true);
  }, [refreshCatalog]);

  return {
    enabled: snapshot.projected.enabled,
    config: snapshot.projected.config,
    catalog,
    loading,
    catalogLoading,
    pending: snapshot.pending,
    saveError: snapshot.error,
    enabledUnavailable: effectiveEnabledUnavailable,
    loadError: effectiveLoadErrorKind !== null,
    loadErrorKind: effectiveLoadErrorKind,
    catalogError,
    toggleEnabled,
    setItemVisible,
    moveItem,
    moveItemBefore,
    setProfileIndependent,
    resetProfile,
    resetAll,
    repairInvalidConfig,
    reloadCatalog,
  };
}
