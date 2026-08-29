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

function getItemLabel(element: Element, fallback: string): string {
  if (element.localName === "menuseparator") return "Separator";
  const label = element.getAttribute("label") ??
    element.getAttribute("aria-label") ??
    element.getAttribute("data-l10n-id") ??
    element.getAttribute("data-lazy-l10n-id") ??
    element.id;
  return label || fallback;
}

function getContainerLabel(surface: ResolvedContextMenuSurface): string {
  if (surface.popup === surface.rootPopup) return surface.adapter.label;
  if (surface.popup.localName === "menugroup") {
    return getItemLabel(surface.popup, surface.containerKey);
  }
  return getItemLabel(
    surface.popup.parentElement ?? surface.popup,
    surface.containerKey,
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
  #revision = 0;

  constructor(registry: ContextMenuRegistry) {
    this.#registry = registry;
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

  record(surface: ResolvedContextMenuSurface): ContextMenuCatalogSnapshot {
    this.recordContainer(surface);
    for (const child of this.#registry.resolveVirtualContainers(surface)) {
      this.recordContainer(child);
    }
    this.#revision++;
    return this.snapshot();
  }

  private recordContainer(surface: ResolvedContextMenuSurface): void {
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
        label: getItemLabel(element, identity.key),
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
    const containers = existingProfile?.containers.map(cloneContainer) ?? [];
    const nextContainer: ContextMenuContainerDescriptor = {
      key: surface.containerKey,
      label: getContainerLabel(surface),
      complete: true,
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
