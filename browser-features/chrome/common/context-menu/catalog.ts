// SPDX-License-Identifier: MPL-2.0

import type {
  ContextMenuRegistry,
  ResolvedContextMenuSurface,
} from "./registry.ts";
import { isNativelyHidden } from "./separator-policy.ts";
import {
  CONTEXT_MENU_SCHEMA_VERSION,
  type ContextMenuCatalogReporter,
  type ContextMenuCatalogSnapshot,
  type ContextMenuContainerDescriptor,
  type ContextMenuItemDescriptor,
  type ContextMenuProfileDescriptor,
  type ContextMenuSurfaceDescriptor,
} from "./types.ts";

const CATALOG_SERVICE_URI =
  "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs";

interface ContextMenuCatalogServiceModule {
  ContextMenuCatalogService?: ContextMenuCatalogReporter;
}

function loadOptionalReporter(): ContextMenuCatalogReporter | null {
  try {
    const module = ChromeUtils.importESModule(
      CATALOG_SERVICE_URI,
    ) as unknown as ContextMenuCatalogServiceModule;
    const reporter = module.ContextMenuCatalogService;
    if (
      reporter &&
      typeof reporter.report === "function" &&
      typeof reporter.removeOwner === "function"
    ) {
      return reporter;
    }
  } catch (error) {
    console.warn(
      "[ContextMenuCustomizer] Catalog service is unavailable; continuing locally",
      error,
    );
  }
  return null;
}

function getLocale(): string {
  try {
    return Services.locale.appLocaleAsBCP47;
  } catch {
    return "und";
  }
}

function getItemLabel(
  element: Element,
  fallback: string,
  localize: (element: Element) => string | null,
): string {
  if (element.localName === "menuseparator") return "";
  for (const attribute of ["label", "aria-label"]) {
    const label = element.getAttribute(attribute)?.trim();
    if (label) return label;
  }
  const localized = localize(element);
  if (localized) return localized;
  for (
    const candidate of [
      element.getAttribute("data-l10n-id"),
      element.getAttribute("data-lazy-l10n-id"),
      element.id,
    ]
  ) {
    const label = candidate?.trim();
    if (label) return label;
  }
  return fallback;
}

function getContainerLabel(
  surface: ResolvedContextMenuSurface,
  localize: (element: Element) => string | null,
): string {
  if (surface.popup === surface.rootPopup) return surface.adapter.label;
  if (surface.popup.localName === "menugroup") {
    return getItemLabel(surface.popup, surface.containerKey, localize);
  }
  return getItemLabel(
    surface.popup.parentElement ?? surface.popup,
    surface.containerKey,
    localize,
  );
}

function cloneContainer(
  container: ContextMenuContainerDescriptor,
): ContextMenuContainerDescriptor {
  return {
    ...container,
    items: container.items.map((item) => ({ ...item })),
  };
}

function cloneProfile(
  profile: ContextMenuProfileDescriptor,
): ContextMenuProfileDescriptor {
  return {
    ...profile,
    containers: profile.containers.map(cloneContainer),
  };
}

function cloneSurface(
  surface: ContextMenuSurfaceDescriptor,
): ContextMenuSurfaceDescriptor {
  return {
    ...surface,
    profiles: surface.profiles.map(cloneProfile),
  };
}

export class OptionalContextMenuCatalogReporter
  implements ContextMenuCatalogReporter {
  readonly #reporter: ContextMenuCatalogReporter | null;

  constructor(
    reporter: ContextMenuCatalogReporter | null = loadOptionalReporter(),
  ) {
    this.#reporter = reporter;
  }

  report(ownerId: string, snapshot: ContextMenuCatalogSnapshot): void {
    try {
      this.#reporter?.report(ownerId, snapshot);
    } catch (error) {
      console.error("[ContextMenuCustomizer] Catalog report failed", error);
    }
  }

  removeOwner(ownerId: string): void {
    try {
      this.#reporter?.removeOwner(ownerId);
    } catch (error) {
      console.error(
        "[ContextMenuCustomizer] Catalog owner cleanup failed",
        error,
      );
    }
  }
}

export class ContextMenuCatalogBuilder {
  readonly #registry: ContextMenuRegistry;
  readonly #surfaces = new Map<string, ContextMenuSurfaceDescriptor>();
  readonly #createLocalization: (
    resources: string[],
  ) => Pick<Localization, "formatMessagesSync">;
  readonly #localizations = new WeakMap<Document, {
    signature: string;
    localization: Pick<Localization, "formatMessagesSync">;
    labels: Map<string, string>;
  }>();
  #revision = 0;

  constructor(
    registry: ContextMenuRegistry,
    createLocalization: (
      resources: string[],
    ) => Pick<Localization, "formatMessagesSync"> = (resources) =>
      new Localization(
        resources.map((path) => ({ path, optional: true })),
        true,
      ),
  ) {
    this.#registry = registry;
    this.#createLocalization = createLocalization;
    for (const adapter of registry.adapters) {
      this.#surfaces.set(adapter.key, {
        key: adapter.key,
        label: adapter.label,
        profiles: adapter.profiles.map((profile) => ({
          key: profile.key,
          label: profile.label,
          containers: [{
            key: "root",
            label: adapter.label,
            complete: false,
            items: [],
          }],
        })),
      });
    }
  }

  private getLabelLocalizer(
    document: Document,
  ): (element: Element) => string | null {
    // Tab labels stay lazy until native tab interaction. Read Fluent directly
    // for the initial catalog without opening or translating Firefox's DOM.
    const resources = [
      ...new Set([
        ...Array.from(document.querySelectorAll('link[rel="localization"]'))
          .map((link) => link.getAttribute("href")?.trim())
          .filter((path): path is string => Boolean(path)),
        "browser/tabContextMenu.ftl",
      ]),
    ];
    const signature = JSON.stringify([
      Services.locale.appLocalesAsBCP47,
      resources,
    ]);

    return (element) => {
      const id = element.getAttribute("data-l10n-id")?.trim() ||
        element.getAttribute("data-lazy-l10n-id")?.trim();
      if (!id || !/^[a-zA-Z][a-zA-Z0-9_-]*$/.test(id)) return null;
      try {
        const serializedArgs = element.getAttribute("data-l10n-args");
        const args: unknown = serializedArgs === null
          ? null
          : JSON.parse(serializedArgs);
        if (
          args !== null &&
          (typeof args !== "object" || Array.isArray(args) ||
            Object.values(args).some((value) =>
              value !== null && typeof value !== "string" &&
              !(typeof value === "number" && Number.isFinite(value))
            ))
        ) return null;

        let cached = this.#localizations.get(document);
        if (!cached || cached.signature !== signature) {
          cached = {
            signature,
            localization: this.#createLocalization(resources),
            labels: new Map(),
          };
          this.#localizations.set(document, cached);
        }
        const key = JSON.stringify([id, args]);
        const existing = cached.labels.get(key);
        if (existing) return existing;
        const message = cached.localization.formatMessagesSync([
          { id, args: args as L10nArgs | null },
        ])[0];
        const label = ["label", "aria-label"].map((name) =>
          message?.attributes?.find((attribute) =>
            attribute.name === name
          )
            ?.value.trim()
        ).find(Boolean) || message?.value?.trim();
        if (!label) return null;
        // Argument values can be generated from arbitrary native context.
        // Bound the cache while retaining the normal static menu labels.
        if (cached.labels.size >= 512) cached.labels.clear();
        cached.labels.set(key, label);
        return label;
      } catch {
        // Invalid metadata or unavailable resources must not drop menu rows.
        return null;
      }
    };
  }

  record(surface: ResolvedContextMenuSurface): ContextMenuCatalogSnapshot {
    this.recordSurface(surface, true);
    this.#revision++;
    return this.snapshot();
  }

  /**
   * Capture the chrome DOM before Firefox has opened the popup for a concrete
   * context. The rows are useful to the Hub immediately, but stay incomplete
   * because popupshowing may still add items or change native visibility.
   */
  seed(surface: ResolvedContextMenuSurface): void {
    this.recordSurface(surface, false);
    this.#revision++;
  }

  private recordSurface(
    surface: ResolvedContextMenuSurface,
    complete: boolean,
  ): void {
    this.recordContainer(surface, complete);
    for (const child of this.#registry.resolveVirtualContainers(surface)) {
      this.recordContainer(child, complete);
    }
  }

  private recordContainer(
    surface: ResolvedContextMenuSurface,
    complete: boolean,
  ): void {
    const localize = this.getLabelLocalizer(surface.popup.ownerDocument);
    const items: ContextMenuItemDescriptor[] = [];
    const elements = Array.from(surface.popup.children);
    const identities = elements.map((element) =>
      this.#registry.identifyItem(surface.adapter, element)
    );
    const keyCounts = new Map<string, number>();
    for (const identity of identities) {
      if (!identity) continue;
      keyCounts.set(identity.key, (keyCounts.get(identity.key) ?? 0) + 1);
    }

    for (let index = 0; index < elements.length; index++) {
      const element = elements[index];
      const identity = identities[index];
      if (!identity) continue;
      const ambiguous = (keyCounts.get(identity.key) ?? 0) > 1;
      items.push({
        key: identity.key,
        catalogInstanceId: `${index}:${identity.key}`,
        label: getItemLabel(element, identity.key, localize),
        kind: identity.kind,
        source: identity.source,
        // Runtime resolution deliberately refuses duplicate keys. Reflect the
        // same rule in the Hub so an ambiguous row never appears draggable or
        // hideable while being a no-op at popup time.
        customizable: ambiguous ? false : identity.customizable,
        movable: ambiguous ? false : identity.movable,
        hideable: ambiguous ? false : identity.hideable,
        orderAnchor: ambiguous ? false : identity.orderAnchor,
        nativeHidden: isNativelyHidden(element),
        ...(identity.childContainerKey
          ? { childContainerKey: identity.childContainerKey }
          : {}),
      });
    }

    const current = this.#surfaces.get(surface.adapter.key) ?? {
      key: surface.adapter.key,
      label: surface.adapter.label,
      profiles: [],
    };
    const profileDefinition = surface.adapter.profiles.find((profile) =>
      profile.key === surface.profileKey
    );
    const existingProfile = current.profiles.find((profile) =>
      profile.key === surface.profileKey
    );
    const existingContainer = existingProfile?.containers.find((container) =>
      container.key === surface.containerKey
    );
    if (
      !complete &&
      (existingContainer?.complete ||
        (existingContainer?.items.length ?? 0) > 0 && items.length === 0)
    ) return;
    const containers = existingProfile?.containers.map(cloneContainer) ?? [];
    const nextContainer: ContextMenuContainerDescriptor = {
      key: surface.containerKey,
      label: getContainerLabel(surface, localize),
      complete,
      items,
    };
    const containerIndex = containers.findIndex((container) =>
      container.key === surface.containerKey
    );
    if (containerIndex === -1) containers.push(nextContainer);
    else containers[containerIndex] = nextContainer;

    const nextProfile: ContextMenuProfileDescriptor = {
      key: surface.profileKey,
      label: profileDefinition?.label ?? surface.profileKey,
      containers,
    };
    const profileIndex = current.profiles.findIndex((profile) =>
      profile.key === surface.profileKey
    );
    const profiles = current.profiles.map(cloneProfile);
    if (profileIndex === -1) profiles.push(nextProfile);
    else profiles[profileIndex] = nextProfile;

    this.#surfaces.set(surface.adapter.key, {
      key: surface.adapter.key,
      label: surface.adapter.label,
      profiles,
    });
  }

  snapshot(): ContextMenuCatalogSnapshot {
    return {
      schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
      revision: this.#revision,
      locale: getLocale(),
      surfaces: [...this.#surfaces.values()].map(cloneSurface),
    };
  }
}
