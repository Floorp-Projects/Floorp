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
    /context(?:-?menu)?$/i.test(element.id);
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

/**
 * Widget classes Firefox paints onto many rows. They do not identify one
 * command, so they must not become a saved key.
 */
const GENERIC_ITEM_CLASSES = new Set([
  "indent",
  "menu-iconic",
  "menu-iconic-auto",
  "menuitem-iconic",
  "menuitem-iconic-auto",
  "subviewbutton",
  "subviewbutton-iconic",
  "subviewbutton-nav",
]);

/**
 * A feature class such as `share-tab-url-item` stays on the node across
 * Firefox releases even when the node has no id. One such class is a stable
 * customization key. Several feature classes are joined so the key still
 * does not depend on sibling order.
 */
function semanticClassKey(element: Element): string | null {
  const names = Array.from(element.classList).filter((name) =>
    !GENERIC_ITEM_CLASSES.has(name) &&
    !name.startsWith("chromeclass-") &&
    /^[a-z][a-z0-9-]*[a-z0-9]$/i.test(name) &&
    name.includes("-")
  ).sort();
  if (names.length === 0) return null;
  return names.join("+");
}

/** Stable across runs. Covers the whole string so a shortened label cannot collide. */
function fnv1a32(text: string): string {
  let hash = 0x811c9dc5;
  for (let index = 0; index < text.length; index++) {
    hash ^= text.charCodeAt(index);
    hash = Math.imul(hash, 0x01000193);
  }
  return (hash >>> 0).toString(16).padStart(8, "0");
}

/**
 * Keep letters from every script. A hash of the original text distinguishes
 * labels that would otherwise share a shortened or punctuated form.
 */
function encodeKeyToken(value: string): string {
  const normalized = value.trim().normalize("NFC");
  const readable = normalized
    .replace(/[^\p{L}\p{N}._-]+/gu, "_")
    .replace(/^_+|_+$/g, "");
  const body = (readable || "item").slice(0, 48);
  return `${body}.${fnv1a32(normalized)}`;
}

/**
 * Identity that survives Firefox rebuilding the row. Prefer attributes that
 * name one command or one tab group over the row's sibling index.
 */
function attributeKey(element: Element): string | null {
  const candidates: Array<[string, string | null]> = [
    ["tab-group", element.getAttribute("tab-group-id")],
    ["profile", element.getAttribute("profileid")],
    ["container", element.getAttribute("data-usercontextid")],
    [
      "l10n",
      element.getAttribute("data-l10n-id") ??
        element.getAttribute("data-lazy-l10n-id"),
    ],
    ["command", element.getAttribute("command")],
    [
      "label",
      element.getAttribute("label") ?? element.getAttribute("aria-label"),
    ],
  ];
  for (const [prefix, value] of candidates) {
    if (!value?.trim()) continue;
    return `${prefix}.${encodeKeyToken(value)}`;
  }
  return null;
}

function siblingLayoutToken(element: Element): string {
  if (element.id) return `#${element.id}`;
  const classKey = semanticClassKey(element);
  if (classKey) return `.${classKey}`;
  const attributes = attributeKey(element);
  if (attributes) return attributes;
  return `anon:${element.localName}`;
}

/**
 * Hash of the menu as Firefox built it. Identification runs after the
 * previous overlay is rolled back, so this is the native row sequence, not
 * the user's order. A saved index key stops matching when that sequence
 * changes, instead of landing on a newly inserted row.
 */
function nativeLayoutFingerprint(element: Element): string {
  const parent = element.parentElement;
  const layout = parent
    ? Array.from(parent.children).map(siblingLayoutToken).join("|")
    : "";
  return fnv1a32(layout);
}

function fallbackKey(adapter: ContextMenuAdapter, element: Element): string {
  if (element.id) {
    const owner = looksLikeExtensionItem(element) ? "extension" : "firefox";
    return `${owner}.${element.id}`;
  }
  const classKey = semanticClassKey(element);
  if (classKey) return `${adapter.key}.class.${classKey}`;
  const attributes = attributeKey(element);
  if (attributes) return `${adapter.key}.${attributes}`;
  const index = Array.from(element.parentElement?.children ?? []).indexOf(
    element,
  );
  return `${adapter.key}.unmanaged.${element.localName}.${
    Math.max(index, 0)
  }.${nativeLayoutFingerprint(element)}`;
}

function itemCapabilities(
  kind: ContextMenuItemKind,
  persistent: boolean,
): Pick<
  ContextMenuItemIdentity,
  "customizable" | "movable" | "hideable" | "orderAnchor"
> {
  return {
    // Preserve the schema-v1 meaning for older Hub code. A separator keeps
    // `customizable` false so a client that only reads that flag does not
    // treat it as an ordinary command. New Hub code uses `hideable`.
    customizable: persistent && kind !== "separator",
    movable: persistent,
    hideable: persistent,
    orderAnchor: persistent,
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
        ...itemCapabilities(kind, true),
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
        // Extension and container rows stay editable. Their alias or id is
        // the saved key, so a later rebuild still finds the same command.
        ...itemCapabilities(kind, true),
        ...(childKey ? { childContainerKey: childKey } : {}),
      };
    }

    const key = fallbackKey(adapter, element);
    const childKey = childContainerKey(kind, key, element);
    return {
      key,
      kind,
      source: extensionItem ? "extension" : "firefox",
      ...itemCapabilities(kind, true),
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
