// SPDX-License-Identifier: MPL-2.0

import { contentContextMenuAdapter } from "./adapters/content.ts";
import { firefoxPopupContextMenuAdapters } from "./adapters/firefox.ts";
import { floorpContextMenuAdapters } from "./adapters/floorp.ts";
import { placesContextMenuAdapter } from "./adapters/places.ts";
import { tabContextMenuAdapter } from "./adapters/tab.ts";
import { toolbarContextMenuAdapter } from "./adapters/toolbar.ts";
import type {
  ContextMenuAdapter,
  ContextMenuItemIdentity,
  ContextMenuItemKind,
} from "./types.ts";

export const FLOORP_CONTEXT_MENU_KEY_ATTRIBUTE = "data-floorp-context-menu-key";

export const ROOT_CONTEXT_MENU_CONTAINER_KEY = "root";

const BROWSER_DOCUMENT_URI = "chrome://browser/content/browser.xhtml";
const GENERIC_BROWSER_SURFACE_PREFIX = "browser.chrome.";
const CONTEXT_MENU_ITEM_LOCAL_NAMES = new Set([
  "menu",
  "menugroup",
  "menuitem",
  "menuseparator",
]);

export const DEFAULT_CONTEXT_MENU_ADAPTERS: readonly ContextMenuAdapter[] = [
  contentContextMenuAdapter,
  tabContextMenuAdapter,
  toolbarContextMenuAdapter,
  placesContextMenuAdapter,
  ...firefoxPopupContextMenuAdapters,
  ...floorpContextMenuAdapters,
];

export interface ResolvedContextMenuSurface {
  adapter: ContextMenuAdapter;
  popup: Element;
  rootPopup: Element;
  profileKey: string;
  containerKey: string;
}

export type ContextMenuItemResolution =
  | { status: "resolved"; element: Element; identity: ContextMenuItemIdentity }
  | { status: "missing" }
  | { status: "ambiguous" };

type ContextMenuItemResolutionPurpose = "visibility" | "ordering";

function matchesAny(element: Element, selectors: readonly string[]): boolean {
  for (const selector of selectors) {
    try {
      if (element.matches(selector)) return true;
    } catch (error) {
      console.error(
        "[ContextMenuCustomizer] Ignoring invalid registry selector",
        selector,
        error,
      );
    }
  }
  return false;
}

function documentUriMatches(
  configuredUris: readonly string[],
  actualUri: string,
): boolean {
  return configuredUris.some((configuredUri) =>
    actualUri === configuredUri ||
    actualUri.startsWith(`${configuredUri}?`) ||
    actualUri.startsWith(`${configuredUri}#`)
  );
}

function isGenericBrowserContextPopup(element: Element): boolean {
  return element.localName === "menupopup" &&
    element.id.length > 0 &&
    element.id.toLocaleLowerCase().includes("context");
}

function findGenericBrowserRootPopup(popup: Element): Element | null {
  let current: Element | null = popup;
  let rootPopup: Element | null = null;
  while (current) {
    if (isGenericBrowserContextPopup(current)) rootPopup = current;
    current = current.parentElement;
  }
  return rootPopup;
}

function genericSurfaceLabel(id: string): string {
  const words = id
    .replace(/([a-z\d])([A-Z])/g, "$1 $2")
    .replace(/[-_:]+/g, " ")
    .replace(/\bcontext\b/gi, " ")
    .replace(/\bmenu\b/gi, " ")
    .replace(/\s+/g, " ")
    .trim();
  if (!words) return id;
  return words[0].toLocaleUpperCase() + words.slice(1);
}

function getKind(element: Element): ContextMenuItemKind {
  switch (element.localName) {
    case "menu":
      return "submenu";
    case "menuseparator":
      return "separator";
    case "menugroup":
      return "group";
    default:
      return "command";
  }
}

function hasDirectMenuPopup(element: Element): boolean {
  return Array.from(element.children).some((child) =>
    child.localName === "menupopup"
  );
}

function childContainerKey(
  kind: ContextMenuItemKind,
  key: string,
  element: Element,
): string | undefined {
  if (kind === "submenu" && hasDirectMenuPopup(element)) {
    return `submenu:${key}`;
  }
  if (kind === "group" && element.children.length > 0) {
    return `group:${key}`;
  }
  return undefined;
}

function looksLikeExtensionItem(element: Element): boolean {
  const id = element.id;
  return element.hasAttribute("ext-type") ||
    (id.includes("-menuitem-") && id.length > "-menuitem-".length);
}

function fallbackKey(adapter: ContextMenuAdapter, element: Element): string {
  if (element.id) return `firefox.${element.id}`;
  const index = Array.from(element.parentElement?.children ?? []).indexOf(
    element,
  );
  return `${adapter.key}.unmanaged.${element.localName}.${Math.max(index, 0)}`;
}

function itemCapabilities(
  kind: ContextMenuItemKind,
  readonlyItem: boolean,
  stableKey: boolean,
): Pick<
  ContextMenuItemIdentity,
  "customizable" | "movable" | "hideable" | "orderAnchor"
> {
  const movable = stableKey && !readonlyItem;
  const hideable = movable && kind !== "separator";
  return {
    // Preserve the schema-v1 meaning for older Hub code. In particular, a
    // separator must not gain a visibility switch merely because it is now
    // movable.
    customizable: movable && hideable,
    movable,
    hideable,
    // Protected Firefox/extension nodes retain their exact native slots. Only
    // user-movable nodes become persisted anchors; anonymous index-derived
    // keys are also excluded because their meaning may change after updates.
    orderAnchor: movable,
  };
}

export class ContextMenuRegistry {
  readonly adapters: readonly ContextMenuAdapter[];
  readonly #genericBrowserAdapters = new Map<string, ContextMenuAdapter>();

  constructor(
    adapters: readonly ContextMenuAdapter[] = DEFAULT_CONTEXT_MENU_ADAPTERS,
  ) {
    this.adapters = adapters;
  }

  resolvePopup(
    popup: Element,
    window: Window,
  ): ResolvedContextMenuSurface | null {
    const documentURI = popup.ownerDocument.documentURI;
    for (const adapter of this.adapters) {
      if (!documentUriMatches(adapter.documentURIs, documentURI)) continue;
      let rootPopup: Element | null = popup;
      while (rootPopup && !matchesAny(rootPopup, adapter.popupSelectors)) {
        rootPopup = rootPopup.parentElement;
      }
      if (!rootPopup) continue;
      const resolved = this.resolveWithAdapter(
        adapter,
        popup,
        rootPopup,
        window,
      );
      if (resolved) return resolved;
    }

    if (!documentUriMatches([BROWSER_DOCUMENT_URI], documentURI)) return null;
    const rootPopup = findGenericBrowserRootPopup(popup);
    if (!rootPopup) return null;
    const adapter = this.getGenericBrowserAdapter(rootPopup.id);
    return this.resolveWithAdapter(adapter, popup, rootPopup, window);
  }

  private resolveWithAdapter(
    adapter: ContextMenuAdapter,
    popup: Element,
    rootPopup: Element,
    window: Window,
  ): ResolvedContextMenuSurface | null {
    let containerKey = ROOT_CONTEXT_MENU_CONTAINER_KEY;
    if (popup !== rootPopup) {
      containerKey = adapter.getContainerKey?.(window, popup, rootPopup) ??
        this.identifyItem(adapter, popup.parentElement ?? popup)
          ?.childContainerKey ??
        "";
      if (!containerKey) return null;
    }
    return {
      adapter,
      popup,
      rootPopup,
      profileKey: adapter.getProfileKey(window, rootPopup),
      containerKey,
    };
  }

  private getGenericBrowserAdapter(rootPopupId: string): ContextMenuAdapter {
    let adapter = this.#genericBrowserAdapters.get(rootPopupId);
    if (adapter) return adapter;

    adapter = {
      key: `${GENERIC_BROWSER_SURFACE_PREFIX}${rootPopupId}`,
      label: genericSurfaceLabel(rootPopupId),
      documentURIs: [BROWSER_DOCUMENT_URI],
      popupSelectors: [`[id="${rootPopupId}"]`],
      aliases: [],
      readonlySelectors: [
        ".customize-context-manageExtension",
        ".unified-extensions-context-menu-pin-to-toolbar",
      ],
      profiles: [{ key: "default", label: "Default" }],
      getProfileKey: () => "default",
    };
    this.#genericBrowserAdapters.set(rootPopupId, adapter);
    return adapter;
  }

  identifyItem(
    adapter: ContextMenuAdapter,
    element: Element,
  ): ContextMenuItemIdentity | null {
    if (!CONTEXT_MENU_ITEM_LOCAL_NAMES.has(element.localName)) return null;

    // Firefox marks page-authored <menu> entries with generateditemid. They
    // belong to the website, not browser chrome, and are intentionally absent
    // from both the catalog and customization resolution.
    if (element.hasAttribute("generateditemid")) return null;

    const kind = getKind(element);
    const floorpKey = element.getAttribute(FLOORP_CONTEXT_MENU_KEY_ATTRIBUTE);
    if (floorpKey) {
      const childKey = childContainerKey(kind, floorpKey, element);
      return {
        key: floorpKey,
        kind,
        source: "floorp",
        ...itemCapabilities(kind, false, true),
        ...(childKey ? { childContainerKey: childKey } : {}),
      };
    }

    const extensionItem = looksLikeExtensionItem(element);

    for (const alias of adapter.aliases) {
      if (!matchesAny(element, alias.selectors)) continue;
      const childKey = childContainerKey(kind, alias.key, element);
      return {
        key: alias.key,
        kind,
        source: extensionItem ? "extension" : alias.source ?? "firefox",
        // An explicit adapter alias is our strongest ownership contract.
        // Firefox uses attributes such as data-usercontextid on several of
        // its own standard commands, so broad fallback selectors must not
        // accidentally turn a known command into a protected item. Extension
        // detection still wins because add-on-owned nodes are never safe
        // persistent targets.
        ...itemCapabilities(kind, extensionItem, true),
        ...(childKey ? { childContainerKey: childKey } : {}),
      };
    }

    const readonlyItem = extensionItem ||
      matchesAny(element, adapter.readonlySelectors);
    const key = fallbackKey(adapter, element);
    const childKey = childContainerKey(kind, key, element);
    const stableKey = Boolean(element.id);
    return {
      key,
      kind,
      source: extensionItem ? "extension" : element.id ? "firefox" : "unknown",
      ...itemCapabilities(kind, readonlyItem, stableKey),
      ...(childKey ? { childContainerKey: childKey } : {}),
    };
  }

  resolveVirtualContainers(
    surface: ResolvedContextMenuSurface,
  ): ResolvedContextMenuSurface[] {
    const result: ResolvedContextMenuSurface[] = [];
    const visit = (container: ResolvedContextMenuSurface): void => {
      for (const element of Array.from(container.popup.children)) {
        const identity = this.identifyItem(container.adapter, element);
        if (identity?.kind !== "group" || !identity.childContainerKey) continue;
        const childSurface: ResolvedContextMenuSurface = {
          adapter: container.adapter,
          popup: element,
          rootPopup: container.rootPopup,
          profileKey: container.profileKey,
          containerKey: identity.childContainerKey,
        };
        result.push(childSurface);
        visit(childSurface);
      }
    };
    visit(surface);
    return result;
  }

  resolveItem(
    surface: ResolvedContextMenuSurface,
    key: string,
  ): ContextMenuItemResolution {
    return this.resolveItemForPurpose(surface, key, "visibility");
  }

  /**
   * Resolve a stable ordering participant. Unlike visibility resolution this
   * intentionally includes movable separators. Protected/read-only nodes are
   * excluded so they retain their exact Firefox-native slots.
   */
  resolveItemForOrdering(
    surface: ResolvedContextMenuSurface,
    key: string,
  ): ContextMenuItemResolution {
    return this.resolveItemForPurpose(surface, key, "ordering");
  }

  private resolveItemForPurpose(
    surface: ResolvedContextMenuSurface,
    key: string,
    purpose: ContextMenuItemResolutionPurpose,
  ): ContextMenuItemResolution {
    const matches: Array<
      { element: Element; identity: ContextMenuItemIdentity }
    > = [];
    for (const element of Array.from(surface.popup.children)) {
      const identity = this.identifyItem(surface.adapter, element);
      if (!identity || identity.key !== key) continue;
      const allowed = purpose === "visibility"
        ? identity.hideable
        : identity.orderAnchor;
      if (!allowed) continue;
      matches.push({ element, identity });
    }

    if (matches.length === 0) return { status: "missing" };
    if (matches.length > 1) return { status: "ambiguous" };
    return { status: "resolved", ...matches[0] };
  }
}
