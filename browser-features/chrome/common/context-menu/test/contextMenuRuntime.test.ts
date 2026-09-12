// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { ContextMenuCatalogBuilder } from "../catalog.ts";
import {
  ContextMenuConfigStore,
  type ContextMenuPreferenceSource,
} from "../config-store.ts";
import {
  DEFAULT_CONTEXT_MENU_CONFIG,
  isContextMenuConfigEmpty,
  parseContextMenuConfigWithStatus,
  resolveContextMenuLevelOverride,
  serializeContextMenuConfig,
} from "../config.ts";
import { ContextMenuController } from "../controller.ts";
import { mergeElementsIntoNativeSlots } from "../order-policy.ts";
import {
  ContextMenuRegistry,
  FLOORP_CONTEXT_MENU_KEY_ATTRIBUTE,
} from "../registry.ts";
import { findSeparatorsToHide, isNativelyHidden } from "../separator-policy.ts";
import {
  FLOORP_CONTEXT_HIDDEN_ATTRIBUTE,
  FLOORP_CONTEXT_SEPARATOR_HIDDEN_ATTRIBUTE,
  FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE,
} from "../style.ts";
import { ContextMenuTransaction } from "../transaction.ts";
import type {
  ContextMenuAdapter,
  ContextMenuCatalogReporter,
  ContextMenuCatalogSnapshot,
  ContextMenuConfig,
} from "../types.ts";

const TEST_POPUP_ID = "floorp-context-menu-runtime-test-popup";
const TEST_SURFACE_KEY = "test.surface";

function appendTestNode(localName: string, id?: string): Element {
  const element = document.createElement(localName);
  if (id) element.id = id;
  return element;
}

function appendPopup(id = TEST_POPUP_ID): Element {
  document.getElementById(id)?.remove();
  const popup = appendTestNode("div", id);
  (document.body ?? document.documentElement).appendChild(popup);
  return popup;
}

function createPopupForDocument(
  documentURI: string,
  id: string,
  parentId?: string,
  className?: string,
): Element {
  const popup = appendTestNode("menupopup", id || undefined);
  if (className) popup.className = className;
  if (parentId) {
    const parent = appendTestNode("toolbarbutton", parentId);
    parent.appendChild(popup);
  }
  const ownerDocument = { documentURI };
  return new Proxy(popup, {
    get(target, property) {
      if (property === "ownerDocument") return ownerDocument;
      const value = Reflect.get(target, property, target);
      return typeof value === "function" ? value.bind(target) : value;
    },
  });
}

function childMarkers(parent: Element): string {
  return Array.from(parent.children).map((element) =>
    element.id || element.getAttribute("data-marker") || element.localName
  ).join(",");
}

function createTestAdapter(): ContextMenuAdapter {
  return {
    key: TEST_SURFACE_KEY,
    label: "Test surface",
    documentURIs: [document.documentURI],
    popupSelectors: [`#${TEST_POPUP_ID}`],
    aliases: [
      { key: "test.a", selectors: ['[id="runtime-a"]'] },
      { key: "test.b", selectors: ['[id="runtime-b"]'] },
      { key: "test.c", selectors: ['[id="runtime-c"]'] },
      { key: "test.separator", selectors: ['[id="runtime-separator"]'] },
      { key: "test.group", selectors: ['[id="runtime-group"]'] },
      { key: "test.submenu", selectors: ['[id="runtime-submenu"]'] },
      { key: "test.child", selectors: ['[id="runtime-child"]'] },
    ],
    readonlySelectors: [],
    profiles: [{ key: "default", label: "Default" }],
    getProfileKey: () => "default",
  };
}

function createConfig(
  level: { order?: string[]; hidden?: string[] },
  containerKey = "root",
): ContextMenuConfig {
  return {
    schemaVersion: 1,
    surfaces: {
      [TEST_SURFACE_KEY]: {
        base: { [containerKey]: level },
        profiles: {},
      },
    },
  };
}

class FakePreferenceSource implements ContextMenuPreferenceSource {
  readonly #serialized: string;
  readonly #enabled: boolean | undefined;

  constructor(config: ContextMenuConfig, enabled?: boolean) {
    this.#serialized = serializeContextMenuConfig(config);
    this.#enabled = enabled;
  }

  getBoolPref(_name: string, defaultValue = false): boolean {
    return this.#enabled ?? defaultValue;
  }

  getStringPref(_name: string, _defaultValue = ""): string {
    return this.#serialized;
  }

  addObserver(_name: string, _observer: nsIObserver): void {}

  removeObserver(_name: string, _observer: nsIObserver): void {}
}

class RecordingReporter implements ContextMenuCatalogReporter {
  readonly reports: ContextMenuCatalogSnapshot[] = [];
  readonly reportOwnerIds: string[] = [];
  readonly removedOwners: string[] = [];

  report(ownerId: string, snapshot: ContextMenuCatalogSnapshot): void {
    this.reportOwnerIds.push(ownerId);
    this.reports.push(snapshot);
  }

  removeOwner(ownerId: string): void {
    this.removedOwners.push(ownerId);
  }
}

function createControllerFixture(config: ContextMenuConfig): {
  controller: ContextMenuController;
  callbacks: Array<() => void>;
  reporter: RecordingReporter;
} {
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  const configStore = new ContextMenuConfigStore(
    new FakePreferenceSource(config),
  );
  const callbacks: Array<() => void> = [];
  const reporter = new RecordingReporter();
  const controller = new ContextMenuController({
    window,
    registry,
    configStore,
    catalogReporter: reporter,
    ownerId: "runtime-test-window",
    scheduleMicrotask: (callback) => callbacks.push(callback),
    scheduleOpeningPass: (callback) => callbacks.push(callback),
  });
  return { controller, callbacks, reporter };
}

function runNextMicrotask(callbacks: Array<() => void>): void {
  const callback = callbacks.shift();
  assert(callback !== undefined, "popupshowing should schedule one microtask");
  callback();
}

function runPopupShowingReconcile(callbacks: Array<() => void>): void {
  runNextMicrotask(callbacks);
}

async function nextAnimationFrame(): Promise<void> {
  await new Promise<void>((resolve) =>
    globalThis.requestAnimationFrame(() => resolve())
  );
}

async function flushMutationObservers(): Promise<void> {
  await Promise.resolve();
  await Promise.resolve();
}

function testConfigParsingAndDormantProfiles(): void {
  assertEquals(
    parseContextMenuConfigWithStatus("").status,
    "empty",
    "an unset pref is an empty v1 config",
  );
  assertEquals(
    parseContextMenuConfigWithStatus('{"schemaVersion":2,"surfaces":{}}')
      .status,
    "unsupported-version",
    "a future schema is distinguished from malformed JSON",
  );
  assertEquals(
    parseContextMenuConfigWithStatus("not-json").status,
    "invalid",
    "malformed JSON is reported without throwing",
  );

  const config: ContextMenuConfig = {
    schemaVersion: 1,
    surfaces: {
      [TEST_SURFACE_KEY]: {
        base: { root: { order: ["base"], hidden: ["base-hidden"] } },
        profiles: {
          default: {
            independent: false,
            containers: {
              root: {
                order: ["profile"],
                hidden: ["profile-hidden"],
              },
            },
          },
        },
      },
    },
  };
  const inherited = resolveContextMenuLevelOverride(
    config,
    TEST_SURFACE_KEY,
    "default",
    "root",
  );
  assertEquals(
    inherited?.order.join(","),
    "base",
    "non-independent profile order remains dormant",
  );
  assertEquals(
    inherited?.hidden.join(","),
    "base-hidden",
    "non-independent profile visibility remains dormant",
  );

  config.surfaces[TEST_SURFACE_KEY].profiles.default.independent = true;
  const independent = resolveContextMenuLevelOverride(
    config,
    TEST_SURFACE_KEY,
    "default",
    "root",
  );
  assertEquals(
    independent?.order.join(","),
    "profile",
    "independent profile uses its retained order",
  );
  assertEquals(
    independent?.hidden.join(","),
    "profile-hidden",
    "independent profile uses its retained visibility",
  );
  assert(
    isContextMenuConfigEmpty(DEFAULT_CONTEXT_MENU_CONFIG),
    "the default config is an explicit no-op",
  );
}

function testConfigStoreDefaultsEnabled(): void {
  const store = new ContextMenuConfigStore(
    new FakePreferenceSource(DEFAULT_CONTEXT_MENU_CONFIG),
  );
  try {
    assertEquals(
      store.getSnapshot().enabled,
      true,
      "the feature is enabled by default while the empty config stays a no-op",
    );
  } finally {
    store.destroy();
  }
}

function testControllerOwnerIdsAreProcessUnique(): void {
  const reporter = new RecordingReporter();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  const first = new ContextMenuController({
    window,
    registry,
    configStore: new ContextMenuConfigStore(
      new FakePreferenceSource(DEFAULT_CONTEXT_MENU_CONFIG),
    ),
    catalogReporter: reporter,
  });
  const second = new ContextMenuController({
    window,
    registry,
    configStore: new ContextMenuConfigStore(
      new FakePreferenceSource(DEFAULT_CONTEXT_MENU_CONFIG),
    ),
    catalogReporter: reporter,
  });
  try {
    first.attach();
    second.attach();
    assert(
      reporter.reportOwnerIds[0] !== reporter.reportOwnerIds[1],
      "default catalog owner IDs use process-wide UUIDs",
    );
  } finally {
    first.destroy();
    second.destroy();
  }
}

function testControllerSeedsInitialPopupWithoutClaimingComplete(): void {
  document.getElementById(TEST_POPUP_ID)?.remove();
  const popup = appendTestNode("menupopup", TEST_POPUP_ID);
  (document.body ?? document.documentElement).appendChild(popup);
  const fixture = createControllerFixture(
    createConfig({ order: ["test.b", "test.a"], hidden: ["test.a"] }),
  );
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, second);

    fixture.controller.attach();
    const seededRoot = fixture.reporter.reports.at(-1)?.surfaces.find(
      (surface) => surface.key === TEST_SURFACE_KEY,
    )?.profiles[0]?.containers.find((container) => container.key === "root");
    assertEquals(
      seededRoot?.complete,
      false,
      "attach-time DOM capture stays provisional until a real popup context is observed",
    );
    assertEquals(
      seededRoot?.items.map((item) => item.key).join(","),
      "test.a,test.b",
      "attach-time DOM capture publishes existing menu rows",
    );
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b",
      "catalog seeding never applies the configured order",
    );
    assert(
      !first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "catalog seeding never applies configured visibility",
    );

    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    runPopupShowingReconcile(fixture.callbacks);
    const observedRoot = fixture.reporter.reports.at(-1)?.surfaces.find(
      (surface) => surface.key === TEST_SURFACE_KEY,
    )?.profiles[0]?.containers.find((container) => container.key === "root");
    assertEquals(
      observedRoot?.complete,
      true,
      "the first real popup observation replaces the provisional catalog",
    );
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-a",
      "normal popup reconciliation still applies the configured order",
    );
  } finally {
    fixture.controller.destroy();
    popup.remove();
  }
}

function testCatalogSeedDoesNotRegressToEmptyClone(): void {
  document.getElementById(TEST_POPUP_ID)?.remove();
  const populatedPopup = appendTestNode("menupopup", TEST_POPUP_ID);
  const emptyClone = appendTestNode("menupopup", TEST_POPUP_ID);
  (document.body ?? document.documentElement).append(
    populatedPopup,
    emptyClone,
  );
  try {
    populatedPopup.appendChild(appendTestNode("menuitem", "runtime-a"));
    const registry = new ContextMenuRegistry([createTestAdapter()]);
    const populatedSurface = registry.resolvePopup(populatedPopup, window);
    const emptySurface = registry.resolvePopup(emptyClone, window);
    assert(populatedSurface !== null, "the populated root resolves");
    assert(emptySurface !== null, "the empty clone resolves");

    const builder = new ContextMenuCatalogBuilder(registry);
    builder.seed(populatedSurface);
    builder.seed(emptySurface);
    const snapshot = builder.snapshot();
    const root = snapshot.surfaces[0]?.profiles[0]?.containers.find(
      (container) => container.key === "root",
    );
    assertEquals(
      root?.items[0]?.key,
      "test.a",
      "an empty clone cannot erase a populated seed for the same logical container",
    );
    assertEquals(root?.complete, false, "the retained seed stays provisional");
  } finally {
    populatedPopup.remove();
    emptyClone.remove();
  }
}

function testNativeSlotMergeAndSameParentGuard(): void {
  const popup = appendPopup();
  const otherParent = appendTestNode("div");
  (document.body ?? document.documentElement).appendChild(otherParent);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const unknown = appendTestNode("menuitem");
    unknown.setAttribute("data-marker", "unknown");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, unknown, second);

    const merged = mergeElementsIntoNativeSlots(popup, [second, first]);
    assert(merged.changed, "managed items should be reordered");
    assertEquals(
      childMarkers(popup),
      "runtime-b,unknown,runtime-a",
      "unknown nodes keep their exact native slot",
    );
    mergeElementsIntoNativeSlots(popup, merged.originalOrder);
    assertEquals(
      childMarkers(popup),
      "runtime-a,unknown,runtime-b",
      "the recorded native order is reversible",
    );

    const foreign = appendTestNode("menuitem", "foreign");
    otherParent.appendChild(foreign);
    const rejected = mergeElementsIntoNativeSlots(popup, [first, foreign]);
    assertEquals(
      rejected.changed,
      false,
      "items from different parents must never be moved",
    );
  } finally {
    popup.remove();
    otherParent.remove();
  }
}

function testRegistryExcludesPageItemsAndKeepsExtensionsReadOnly(): void {
  const adapter = createTestAdapter();
  const registry = new ContextMenuRegistry([adapter]);
  const script = appendTestNode("script");
  assertEquals(
    registry.identifyItem(adapter, script),
    null,
    "non-menu popup children are excluded from the catalog",
  );

  const pageItem = appendTestNode("menuitem");
  pageItem.setAttribute("generateditemid", "page-menu-id");
  assertEquals(
    registry.identifyItem(adapter, pageItem),
    null,
    "page-authored generated menu items are excluded from the catalog",
  );

  const extension = appendTestNode("menuitem", "addon-command");
  extension.setAttribute("ext-type", "context-menu");
  const extensionIdentity = registry.identifyItem(adapter, extension);
  assert(extensionIdentity !== null, "extension items remain in the catalog");
  assertEquals(
    extensionIdentity.source,
    "extension",
    "ext-type identifies an extension-owned item",
  );
  assertEquals(
    extensionIdentity.customizable,
    false,
    "extension-owned items are read-only",
  );
  assertEquals(
    extensionIdentity.orderAnchor,
    false,
    "extension IDs are not persisted as stable order anchors",
  );

  const extensionById = appendTestNode(
    "menuitem",
    "addon-widget-menuitem-command",
  );
  const extensionByIdIdentity = registry.identifyItem(adapter, extensionById);
  assertEquals(
    extensionByIdIdentity?.source,
    "extension",
    "the WebExtension widget menuitem ID pattern is protected without ext-type",
  );
  assertEquals(
    extensionByIdIdentity?.customizable,
    false,
    "ID-detected extension items remain read-only",
  );

  const floorp = appendTestNode("menuitem");
  floorp.setAttribute(FLOORP_CONTEXT_MENU_KEY_ATTRIBUTE, "floorp.test");
  const floorpIdentity = registry.identifyItem(adapter, floorp);
  assertEquals(
    floorpIdentity?.source,
    "floorp",
    "Floorp's stable key attribute identifies owned commands",
  );
  assertEquals(
    floorpIdentity?.customizable,
    true,
    "Floorp-owned commands are customizable",
  );

  const separator = appendTestNode("menuseparator", "runtime-separator");
  const separatorIdentity = registry.identifyItem(adapter, separator);
  assertEquals(
    separatorIdentity?.movable,
    true,
    "a known separator with a semantic key is movable",
  );
  assertEquals(
    separatorIdentity?.hideable,
    false,
    "a movable separator does not gain a manual visibility override",
  );
  assertEquals(
    separatorIdentity?.customizable,
    false,
    "the legacy combined capability remains false for separators",
  );
  assertEquals(
    separatorIdentity?.orderAnchor,
    true,
    "a known separator can be used as a stable order anchor",
  );

  const anonymousSeparator = appendTestNode("menuseparator");
  const anonymousIdentity = registry.identifyItem(
    adapter,
    anonymousSeparator,
  );
  assertEquals(
    anonymousIdentity?.movable,
    false,
    "an index-keyed anonymous separator is not movable",
  );
  assertEquals(
    anonymousIdentity?.orderAnchor,
    false,
    "an index-keyed anonymous separator is not persisted as an anchor",
  );
}

function testExplicitAliasesOverrideBroadReadonlyFallbacks(): void {
  const adapter: ContextMenuAdapter = {
    ...createTestAdapter(),
    readonlySelectors: ["[data-usercontextid]"],
  };
  const registry = new ContextMenuRegistry([adapter]);

  const knownFirefoxItem = appendTestNode("menuitem", "runtime-a");
  knownFirefoxItem.setAttribute("data-usercontextid", "0");
  const knownIdentity = registry.identifyItem(adapter, knownFirefoxItem);
  assertEquals(
    knownIdentity?.movable,
    true,
    "an explicit Firefox alias remains movable despite a broad fallback attribute",
  );
  assertEquals(
    knownIdentity?.hideable,
    true,
    "an explicit Firefox alias remains hideable despite a broad fallback attribute",
  );

  const unknownDynamicItem = appendTestNode(
    "menuitem",
    "runtime-dynamic-container-command",
  );
  unknownDynamicItem.setAttribute("data-usercontextid", "7");
  const unknownIdentity = registry.identifyItem(adapter, unknownDynamicItem);
  assertEquals(
    unknownIdentity?.movable,
    false,
    "a non-aliased dynamic container item remains protected",
  );

  const extensionAlias = appendTestNode("menuitem", "runtime-b");
  extensionAlias.setAttribute("ext-type", "context-menu");
  assertEquals(
    registry.identifyItem(adapter, extensionAlias)?.movable,
    false,
    "extension ownership still overrides an explicit selector alias",
  );
}

function testCatalogUsesLocalizedAccessibleLabels(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  const builder = new ContextMenuCatalogBuilder(registry);
  try {
    const item = appendTestNode("menuitem", "runtime-a");
    item.setAttribute("label", "");
    item.setAttribute("data-l10n-id", "raw-l10n-message-id");
    item.setAttribute("aria-label", "Localized command");
    const separator = appendTestNode("menuseparator", "runtime-separator");
    popup.append(item, separator);

    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");
    const snapshot = builder.record(surface);
    assertEquals(
      snapshot.surfaces[0]?.profiles[0]?.containers[0]?.items[0]?.label,
      "Localized command",
      "the Hub catalog prefers Firefox's localized accessible label",
    );
    assertEquals(
      snapshot.surfaces[0]?.profiles[0]?.containers[0]?.items[1]?.label,
      "",
      "separator labels stay empty so the Hub can use its localized fallback",
    );
  } finally {
    popup.remove();
  }
}

function testCatalogProtectsDuplicateStableKeys(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  const builder = new ContextMenuCatalogBuilder(registry);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    first.setAttribute("label", "First duplicate");
    const second = appendTestNode("menuitem", "runtime-a");
    second.setAttribute("label", "Second duplicate");
    popup.append(first, second);

    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "duplicate-key popup should resolve");
    const items = builder.record(surface).surfaces[0]?.profiles[0]
      ?.containers[0]?.items ?? [];
    assertEquals(items.length, 2, "both duplicate native rows stay visible");
    assert(
      items.every((item) =>
        !item.movable && !item.hideable && !item.orderAnchor
      ),
      "every ambiguous catalog row is protected consistently with runtime resolution",
    );
    assert(
      items[0]?.catalogInstanceId !== items[1]?.catalogInstanceId,
      "duplicate rows receive distinct snapshot-local rendering identities",
    );
    assertEquals(
      registry.resolveItemForOrdering(surface, "test.a").status,
      "ambiguous",
      "runtime ordering rejects the same duplicate key",
    );
  } finally {
    popup.remove();
  }
}

function testRegistryGenericBrowserContextFallback(): void {
  if (
    !document.documentURI.startsWith(
      "chrome://browser/content/browser.xhtml",
    )
  ) {
    return;
  }

  const registry = new ContextMenuRegistry();
  const popup = appendTestNode("menupopup", "future-context-menu");
  const command = appendTestNode("menuitem", "future-context-command");
  const submenu = appendTestNode("menu", "future-context-submenu");
  const nestedPopup = appendTestNode(
    "menupopup",
    "future-context-submenu-popup",
  );
  submenu.appendChild(nestedPopup);
  popup.append(command, submenu);
  (document.body ?? document.documentElement).appendChild(popup);
  try {
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "generic browser popup should resolve");
    assertEquals(
      surface.adapter.key,
      "browser.chrome.future-context-menu",
      "an unknown browser.xhtml context popup receives a stable fallback surface",
    );
    assertEquals(
      registry.identifyItem(surface.adapter, command)?.key,
      "firefox.future-context-command",
      "generic fallback retains stable native item IDs",
    );
    const nestedSurface = registry.resolvePopup(nestedPopup, window);
    assert(nestedSurface !== null, "generic nested popup should resolve");
    assertEquals(
      nestedSurface.adapter.key,
      "browser.chrome.future-context-menu",
      "a context-named child popup remains under the outer fallback surface",
    );
    assertEquals(
      nestedSurface.containerKey,
      "submenu:firefox.future-context-submenu",
      "generic fallback retains nested container structure",
    );
  } finally {
    popup.remove();
  }

  for (
    const firefoxContextId of [
      "SyncedTabsSidebarContext",
      "SyncedTabsSidebarTabsFilterContext",
    ]
  ) {
    const firefoxContext = appendTestNode("menupopup", firefoxContextId);
    (document.body ?? document.documentElement).appendChild(firefoxContext);
    try {
      assert(
        registry.resolvePopup(firefoxContext, window) !== null,
        `${firefoxContextId} remains covered by the generic context fallback`,
      );
    } finally {
      firefoxContext.remove();
    }
  }
}

function testKnownFirefoxPopupSurfacesAreDocumentScoped(): void {
  const browserDocumentURI = "chrome://browser/content/browser.xhtml";
  const placesDocumentURI = "chrome://browser/content/places/places.xhtml";
  const registry = new ContextMenuRegistry();
  const builder = new ContextMenuCatalogBuilder(registry);
  const contracts = [
    [browserDocumentURI, "backForwardMenu", "browser.navigation-history"],
    [browserDocumentURI, "new-tab-button-popup", "browser.new-tab-button"],
    [browserDocumentURI, "downloadsContextMenu", "browser.downloads"],
    [browserDocumentURI, "split-view-menu", "browser.split-view"],
    [placesDocumentURI, "placesColumnsContext", "places.library-columns"],
    [placesDocumentURI, "downloadsContextMenu", "places.library-downloads"],
  ] as const;

  for (const [documentURI, popupId, surfaceKey] of contracts) {
    const popup = createPopupForDocument(documentURI, popupId);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, `${documentURI}#${popupId} must resolve`);
    assertEquals(
      surface.adapter.key,
      surfaceKey,
      `${documentURI}#${popupId} must keep its document-scoped surface`,
    );
    builder.record(surface);
  }

  const snapshot = builder.snapshot();
  const browserDownloads = snapshot.surfaces.find((surface) =>
    surface.key === "browser.downloads"
  );
  const libraryDownloads = snapshot.surfaces.find((surface) =>
    surface.key === "places.library-downloads"
  );
  assert(
    browserDownloads !== undefined && libraryDownloads !== undefined,
    "the duplicate downloadsContextMenu id must produce two catalog surfaces",
  );
  assertEquals(
    browserDownloads.profiles[0]?.containers[0]?.complete,
    true,
    "browser downloads catalog is independently complete",
  );
  assertEquals(
    libraryDownloads.profiles[0]?.containers[0]?.complete,
    true,
    "Library downloads catalog is independently complete",
  );

  const unknownPlacesPopup = createPopupForDocument(
    placesDocumentURI,
    "unregisteredPopup",
  );
  assertEquals(
    registry.resolvePopup(unknownPlacesPopup, window),
    null,
    "an arbitrary Library menupopup must not be captured",
  );
  const unknownBrowserPopup = createPopupForDocument(
    browserDocumentURI,
    "unregisteredPopup",
  );
  assertEquals(
    registry.resolvePopup(unknownBrowserPopup, window),
    null,
    "an arbitrary browser menupopup must not be captured",
  );
  for (
    const ordinaryPopupId of [
      "menu_newUserContextPopup",
      "userContext-indicator-menu",
    ]
  ) {
    const ordinaryPopup = createPopupForDocument(
      browserDocumentURI,
      ordinaryPopupId,
    );
    assertEquals(
      registry.resolvePopup(ordinaryPopup, window),
      null,
      `${ordinaryPopupId} is an ordinary left-click menu, not a context menu`,
    );
  }
}

function testFirefoxPopupCloneSelectorsStayNarrow(): void {
  const browserDocumentURI = "chrome://browser/content/browser.xhtml";
  const registry = new ContextMenuRegistry();
  for (
    const parentId of [
      "new-tab-button",
      "tabs-newtab-button",
      "vertical-tabs-newtab-button",
    ]
  ) {
    const popup = createPopupForDocument(
      browserDocumentURI,
      "",
      parentId,
      "new-tab-popup",
    );
    assertEquals(
      registry.resolvePopup(popup, window)?.adapter.key,
      "browser.new-tab-button",
      `Firefox's id-less new-tab clone under #${parentId} must resolve`,
    );
  }

  for (const parentId of ["back-button", "forward-button"]) {
    const popup = createPopupForDocument(
      browserDocumentURI,
      "",
      parentId,
    );
    popup.setAttribute("context", "");
    assertEquals(
      registry.resolvePopup(popup, window)?.adapter.key,
      "browser.navigation-history",
      `Firefox's id-less history clone under #${parentId} must resolve`,
    );
  }

  const unrelatedClone = createPopupForDocument(
    browserDocumentURI,
    "",
    "unrelated-button",
    "new-tab-popup",
  );
  assertEquals(
    registry.resolvePopup(unrelatedClone, window),
    null,
    "the clone class alone must not opt arbitrary popups into customization",
  );
}

function testCurrentFirefoxContextMenuContracts(): void {
  if (
    !document.documentURI.startsWith(
      "chrome://browser/content/browser.xhtml",
    )
  ) {
    return;
  }

  const registry = new ContextMenuRegistry();
  for (
    const [popupId, surfaceKey] of [
      ["contentAreaContextMenu", "browser.content"],
      ["tabContextMenu", "browser.tabs"],
      ["toolbar-context-menu", "browser.toolbar"],
      ["placesContext", "browser.places"],
      ["split-view-menu", "browser.split-view"],
    ] as const
  ) {
    const popup = document.getElementById(popupId);
    assert(popup !== null, `Firefox chrome contract #${popupId} must exist`);
    assertEquals(
      registry.resolvePopup(popup, window)?.adapter.key,
      surfaceKey,
      `#${popupId} must resolve to ${surfaceKey}`,
    );
  }

  const aliasContracts = [
    ["context-openlinkintab", "content.link.open-new-tab"],
    ["context-openlinkinsplitview", "content.link.open-split-view"],
    ["context_reloadTab", "tab.reload"],
    ["placesContext_open", "places.open"],
  ] as const;
  for (const [elementId, expectedKey] of aliasContracts) {
    const element = document.getElementById(elementId);
    assert(
      element !== null,
      `Firefox chrome contract #${elementId} must exist`,
    );
    const popup = element.closest("menupopup");
    assert(popup !== null, `#${elementId} must belong to a menupopup`);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, `#${elementId} popup must resolve`);
    assertEquals(
      registry.identifyItem(surface.adapter, element)?.key,
      expectedKey,
      `#${elementId} must retain its semantic alias`,
    );
  }

  const toolbarOverflow = document.getElementById(
    "toolbar-context-move-to-panel",
  ) ?? document.getElementById("toolbar-context-pinToOverflowMenu");
  assert(
    toolbarOverflow !== null,
    "Firefox toolbar overflow command must match a current or legacy ID",
  );
  const toolbarPopup = document.getElementById("toolbar-context-menu");
  assert(toolbarPopup !== null, "Firefox toolbar popup must exist");
  const toolbarSurface = registry.resolvePopup(toolbarPopup, window);
  assert(toolbarSurface !== null, "Firefox toolbar popup must resolve");
  assertEquals(
    registry.identifyItem(toolbarSurface.adapter, toolbarOverflow)?.key,
    "toolbar.pin-overflow",
    "current and legacy toolbar IDs share one semantic key",
  );
  const extensionAnchor = document.getElementById(
    "toolbar-context-manage-extension",
  );
  assert(
    extensionAnchor !== null,
    "Firefox asynchronous extension controls retain their insertion anchor",
  );
  assertEquals(
    registry.identifyItem(toolbarSurface.adapter, extensionAnchor)
      ?.customizable,
    false,
    "the asynchronous extension-control anchor stays in its native slot",
  );

  const navigationGroup = document.getElementById("context-navigation");
  assert(
    navigationGroup !== null && navigationGroup.localName === "menugroup",
    "Firefox content navigation remains a menugroup contract",
  );
  const contentPopup = document.getElementById("contentAreaContextMenu");
  assert(contentPopup !== null, "Firefox content popup must exist");
  const contentSurface = registry.resolvePopup(contentPopup, window);
  assert(contentSurface !== null, "Firefox content popup must resolve");
  assertEquals(
    registry.identifyItem(contentSurface.adapter, navigationGroup)
      ?.childContainerKey,
    "group:content.navigation",
    "the Firefox navigation menugroup must remain an editable child container",
  );
}

function testTransactionUsesOverlayAndRollsBack(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const unknown = appendTestNode("menuitem");
    unknown.setAttribute("data-marker", "unknown");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, unknown, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a"],
      hidden: ["test.a"],
    });
    assert(transaction.apply(), "known overrides should apply");
    assertEquals(
      childMarkers(popup),
      "runtime-b,unknown,runtime-a",
      "transaction uses native-slot merge",
    );
    assert(
      first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "visibility uses Floorp's transient attribute",
    );
    assert(
      !first.hasAttribute("hidden"),
      "the native hidden attribute is untouched",
    );
    assert(
      window.getComputedStyle(first)?.display === "none",
      "the privileged document stylesheet applies the visibility overlay",
    );
    assertEquals(
      document.querySelector("[data-floorp-context-menu-style]"),
      null,
      "Gecko documents do not rely on an inline style element",
    );

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,unknown,runtime-b",
      "popuphiding rollback restores native slots",
    );
    assert(
      !first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "rollback removes Floorp's visibility overlay",
    );
    assert(
      window.getComputedStyle(first)?.display !== "none",
      "the last rollback releases the privileged document stylesheet",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionRollbackIsolatesRestoreFailures(): void {
  const popup = appendPopup();
  const baseAdapter = createTestAdapter();
  const adapter: ContextMenuAdapter = {
    ...baseAdapter,
    aliases: [
      ...baseAdapter.aliases,
      { key: "test.d", selectors: ['[id="runtime-d"]'] },
    ],
  };
  const registry = new ContextMenuRegistry([adapter]);
  const nativeMutationObserver = window.MutationObserver;
  const mutationObserverDescriptor = Object.getOwnPropertyDescriptor(
    window,
    "MutationObserver",
  );
  const observerStates: Array<{ disconnected: boolean }> = [];
  class RecordingMutationObserver {
    readonly #observer: MutationObserver;
    readonly #state = { disconnected: false };

    constructor(callback: MutationCallback) {
      this.#observer = new nativeMutationObserver(callback);
      observerStates.push(this.#state);
    }

    observe(target: Node, options: MutationObserverInit): void {
      this.#observer.observe(target, options);
    }

    takeRecords(): MutationRecord[] {
      return this.#observer.takeRecords();
    }

    disconnect(): void {
      this.#state.disconnected = true;
      this.#observer.disconnect();
    }
  }

  Object.defineProperty(window, "MutationObserver", {
    configurable: true,
    value: RecordingMutationObserver,
  });

  const originalResolveForOrdering = registry.resolveItemForOrdering.bind(
    registry,
  );
  const originalConsoleError = console.error;
  const rollbackErrors: unknown[][] = [];
  let transaction: ContextMenuTransaction | null = null;
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const separator = appendTestNode("menuseparator", "runtime-separator");
    separator.setAttribute(FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE, "true");
    (separator as Element & { hidden: boolean }).hidden = true;
    const group = appendTestNode("menugroup", "runtime-group");
    const childFirst = appendTestNode("menuitem", "runtime-c");
    const childSecond = appendTestNode("menuitem", "runtime-d");
    group.append(childFirst, childSecond);
    popup.append(first, second, separator, group);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const nestedElements = new Map<string, Element>([
      ["test.c", childFirst],
      ["test.d", childSecond],
    ]);
    registry.resolveItemForOrdering = (candidateSurface, key) => {
      const element = nestedElements.get(key);
      if (!element) return originalResolveForOrdering(candidateSurface, key);
      const identity = registry.identifyItem(candidateSurface.adapter, element);
      assert(identity !== null, "nested test item should have an identity");
      return { status: "resolved", element, identity };
    };

    transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a", "test.d", "test.c"],
      hidden: ["test.a"],
    });
    assert(transaction.apply(), "both parent overlays should apply");
    assertEquals(
      observerStates.length,
      2,
      "each reordered parent should be observed",
    );
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-a,runtime-separator,runtime-group",
      "the root overlay should reorder its managed items",
    );
    assertEquals(
      childMarkers(group),
      "runtime-d,runtime-c",
      "the nested overlay should reorder its managed items",
    );
    assertEquals(
      (separator as Element & { hidden: boolean }).hidden,
      false,
      "the legacy separator property should be overlaid",
    );

    childFirst.remove();
    registry.resolveItemForOrdering = (candidateSurface, key) => {
      if (key === "test.c") {
        throw new Error("intentional nested restore failure");
      }
      return originalResolveForOrdering(candidateSurface, key);
    };
    console.error = (...args: unknown[]) => rollbackErrors.push(args);
    transaction.rollback();
    console.error = originalConsoleError;

    assert(
      observerStates.every((state) => state.disconnected),
      "all observers disconnect before a per-parent restore can fail",
    );
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b,runtime-separator,runtime-group",
      "a failing nested restore does not prevent another parent rollback",
    );
    assert(
      !first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "attribute cleanup continues after a parent restore failure",
    );
    assertEquals(
      (separator as Element & { hidden: boolean }).hidden,
      true,
      "hidden-property cleanup continues after a parent restore failure",
    );
    assert(
      window.getComputedStyle(first)?.display !== "none",
      "stylesheet cleanup continues after a parent restore failure",
    );
    assert(
      rollbackErrors.some((args) =>
        String(args[0]).includes("native-order restore")
      ),
      "the isolated restore failure should be reported",
    );
  } finally {
    console.error = originalConsoleError;
    registry.resolveItemForOrdering = originalResolveForOrdering;
    transaction?.rollback();
    if (mutationObserverDescriptor) {
      Object.defineProperty(
        window,
        "MutationObserver",
        mutationObserverDescriptor,
      );
    } else {
      Reflect.deleteProperty(window, "MutationObserver");
    }
    popup.remove();
  }
}

function testTransactionMovesAcrossKnownSeparator(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const prefix = appendTestNode("menuitem", "runtime-c");
    const first = appendTestNode("menuitem", "runtime-a");
    const newFirefoxItem = appendTestNode("menuitem", "runtime-new-item");
    const separator = appendTestNode("menuseparator", "runtime-separator");
    separator.setAttribute(FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE, "true");
    (separator as Element & { hidden: boolean }).hidden = true;
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(prefix, first, newFirefoxItem, separator, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.separator", "test.a", "test.b"],
      // A hand-edited preference must not turn a movable separator into a
      // manually hidden item.
      hidden: ["test.separator"],
    });
    assert(transaction.apply(), "separator-relative order should apply");
    assertEquals(
      childMarkers(popup),
      "runtime-c,runtime-separator,runtime-new-item,runtime-a,runtime-b",
      "a command crosses the separator while a new unconfigured Firefox item keeps its native slot",
    );
    assert(
      !separator.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "separator visibility cannot be overridden through a stale preference",
    );
    assertEquals(
      (separator as Element & { hidden: boolean }).hidden,
      false,
      "custom ordering recomputes a separator hidden by the legacy native-order cleanup",
    );

    const replacementSeparator = appendTestNode(
      "menuseparator",
      "runtime-separator",
    );
    separator.replaceWith(replacementSeparator);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-c,runtime-a,runtime-new-item,runtime-separator,runtime-b",
      "a same-key separator replacement rolls back to Firefox's native slot",
    );
    assertEquals(
      popup.children[3],
      replacementSeparator,
      "rollback adopts Firefox's replacement separator without reviving the old node",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionKeepsProtectedItemInNativeSlot(): void {
  const popup = appendPopup();
  const adapter: ContextMenuAdapter = {
    ...createTestAdapter(),
    readonlySelectors: ['[id="runtime-protected"]'],
  };
  const registry = new ContextMenuRegistry([adapter]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const protectedItem = appendTestNode("menuitem", "runtime-protected");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, protectedItem, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      // Include the protected key as if a preference had been hand-edited.
      order: ["test.b", "firefox.runtime-protected", "test.a"],
      hidden: ["firefox.runtime-protected"],
    });
    assert(transaction.apply(), "movable item order should still apply");
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-protected,runtime-a",
      "the protected node keeps its exact native slot",
    );
    assert(
      !protectedItem.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "the protected node cannot receive a visibility overlay",
    );

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-protected,runtime-b",
      "rollback preserves the protected native slot",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionRestoresSurvivorsAfterNativeNodeChanges(): void {
  for (const mutation of ["remove", "reparent", "replace"] as const) {
    const popup = appendPopup();
    const otherParent = appendTestNode("div");
    (document.body ?? document.documentElement).appendChild(otherParent);
    const registry = new ContextMenuRegistry([createTestAdapter()]);
    try {
      const first = appendTestNode("menuitem", "runtime-a");
      const unknown = appendTestNode("menuitem");
      unknown.setAttribute("data-marker", "unknown");
      const second = appendTestNode("menuitem", "runtime-b");
      const third = appendTestNode("menuitem", "runtime-c");
      popup.append(first, unknown, second, third);
      const surface = registry.resolvePopup(popup, window);
      assert(surface !== null, "test popup should resolve");

      const transaction = new ContextMenuTransaction(surface, registry, {
        order: ["test.c", "test.b", "test.a"],
        hidden: [],
      });
      assert(transaction.apply(), `${mutation}: overlay should apply`);
      assertEquals(
        childMarkers(popup),
        "runtime-c,unknown,runtime-b,runtime-a",
        `${mutation}: precondition uses the customized order`,
      );

      if (mutation === "remove") {
        second.remove();
      } else if (mutation === "reparent") {
        otherParent.appendChild(second);
      } else {
        const replacement = appendTestNode("menuitem");
        replacement.setAttribute("data-marker", "replacement");
        second.replaceWith(replacement);
      }

      transaction.rollback();
      assertEquals(
        childMarkers(popup),
        mutation === "replace"
          ? "runtime-a,unknown,replacement,runtime-c"
          : "runtime-a,unknown,runtime-c",
        `${mutation}: surviving managed nodes return to native order`,
      );
      if (mutation === "reparent") {
        assertEquals(
          second.parentElement,
          otherParent,
          "rollback must not pull a natively reparented node back",
        );
      }
    } finally {
      popup.remove();
      otherParent.remove();
    }
  }
}

function testTransactionRestoresSingleSurvivorToNativeSlot(): void {
  for (const mutation of ["remove", "reparent"] as const) {
    const popup = appendPopup();
    const otherParent = appendTestNode("div");
    (document.body ?? document.documentElement).appendChild(otherParent);
    const registry = new ContextMenuRegistry([createTestAdapter()]);
    try {
      const first = appendTestNode("menuitem", "runtime-a");
      const nativeAnchor = appendTestNode("menuitem");
      nativeAnchor.setAttribute("data-marker", "native-anchor");
      const second = appendTestNode("menuitem", "runtime-b");
      popup.append(first, nativeAnchor, second);
      const surface = registry.resolvePopup(popup, window);
      assert(surface !== null, "test popup should resolve");

      const transaction = new ContextMenuTransaction(surface, registry, {
        order: ["test.b", "test.a"],
        hidden: [],
      });
      assert(transaction.apply(), `${mutation}: overlay should apply`);
      assertEquals(
        childMarkers(popup),
        "runtime-b,native-anchor,runtime-a",
        `${mutation}: precondition uses the customized order`,
      );

      if (mutation === "remove") second.remove();
      else otherParent.appendChild(second);

      transaction.rollback();
      assertEquals(
        childMarkers(popup),
        "runtime-a,native-anchor",
        `${mutation}: the last survivor returns to its native unmanaged slot`,
      );
      if (mutation === "reparent") {
        assertEquals(
          second.parentElement,
          otherParent,
          "rollback must not pull the reparented node back",
        );
      }
    } finally {
      popup.remove();
      otherParent.remove();
    }
  }
}

function testTransactionRestoresSurvivorsAcrossNativeRegions(): void {
  for (const mutation of ["remove", "reparent"] as const) {
    const popup = appendPopup();
    const otherParent = appendTestNode("div");
    (document.body ?? document.documentElement).appendChild(otherParent);
    const registry = new ContextMenuRegistry([createTestAdapter()]);
    try {
      const first = appendTestNode("menuitem", "runtime-a");
      const firstAnchor = appendTestNode("menuitem");
      firstAnchor.setAttribute("data-marker", "first-anchor");
      const second = appendTestNode("menuitem", "runtime-b");
      const secondAnchor = appendTestNode("menuitem");
      secondAnchor.setAttribute("data-marker", "second-anchor");
      const third = appendTestNode("menuitem", "runtime-c");
      popup.append(first, firstAnchor, second, secondAnchor, third);
      const surface = registry.resolvePopup(popup, window);
      assert(surface !== null, "test popup should resolve");

      const transaction = new ContextMenuTransaction(surface, registry, {
        order: ["test.c", "test.b", "test.a"],
        hidden: [],
      });
      assert(transaction.apply(), `${mutation}: overlay should apply`);
      assertEquals(
        childMarkers(popup),
        "runtime-c,first-anchor,runtime-b,second-anchor,runtime-a",
        `${mutation}: precondition spans multiple unmanaged regions`,
      );

      if (mutation === "remove") first.remove();
      else otherParent.appendChild(first);

      transaction.rollback();
      assertEquals(
        childMarkers(popup),
        "first-anchor,runtime-b,second-anchor,runtime-c",
        `${mutation}: every survivor returns to its own native region`,
      );
      if (mutation === "reparent") {
        assertEquals(
          first.parentElement,
          otherParent,
          "rollback must not pull the reparented node back",
        );
      }
    } finally {
      popup.remove();
      otherParent.remove();
    }
  }
}

function testTransactionRestoresAfterNativeBoundaryChanges(): void {
  for (const mutation of ["remove", "reparent"] as const) {
    const popup = appendPopup();
    const otherParent = appendTestNode("div");
    (document.body ?? document.documentElement).appendChild(otherParent);
    const registry = new ContextMenuRegistry([createTestAdapter()]);
    try {
      const first = appendTestNode("menuitem", "runtime-a");
      const firstAnchor = appendTestNode("menuitem");
      firstAnchor.setAttribute("data-marker", "first-anchor");
      const second = appendTestNode("menuitem", "runtime-b");
      const secondAnchor = appendTestNode("menuitem");
      secondAnchor.setAttribute("data-marker", "second-anchor");
      const third = appendTestNode("menuitem", "runtime-c");
      popup.append(first, firstAnchor, second, secondAnchor, third);
      const surface = registry.resolvePopup(popup, window);
      assert(surface !== null, "test popup should resolve");

      const transaction = new ContextMenuTransaction(surface, registry, {
        order: ["test.c", "test.b", "test.a"],
        hidden: [],
      });
      assert(transaction.apply(), `${mutation}: overlay should apply`);
      assertEquals(
        childMarkers(popup),
        "runtime-c,first-anchor,runtime-b,second-anchor,runtime-a",
        `${mutation}: precondition spans the boundary that will disappear`,
      );

      if (mutation === "remove") secondAnchor.remove();
      else otherParent.appendChild(secondAnchor);

      transaction.rollback();
      assertEquals(
        childMarkers(popup),
        "runtime-a,first-anchor,runtime-b,runtime-c",
        `${mutation}: adjacent native regions merge in their original order`,
      );
      if (mutation === "reparent") {
        assertEquals(
          secondAnchor.parentElement,
          otherParent,
          "rollback must not pull the reparented boundary back",
        );
      }
    } finally {
      popup.remove();
      otherParent.remove();
    }
  }
}

function testTransactionRestoresAfterCombinedNativeChanges(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const firstAnchor = appendTestNode("menuitem");
    firstAnchor.setAttribute("data-marker", "first-anchor");
    const second = appendTestNode("menuitem", "runtime-b");
    const secondAnchor = appendTestNode("menuitem");
    secondAnchor.setAttribute("data-marker", "second-anchor");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, firstAnchor, second, secondAnchor, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    first.remove();
    const boundaryReplacement = appendTestNode("menuitem");
    boundaryReplacement.setAttribute("data-marker", "boundary-replacement");
    secondAnchor.replaceWith(boundaryReplacement);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "first-anchor,runtime-b,boundary-replacement,runtime-c",
      "combined managed removal and boundary replacement preserve Firefox's new node",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesNativeBoundaryMove(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, nativeBoundary, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");
    assertEquals(
      childMarkers(popup),
      "runtime-b,native-boundary,runtime-a",
      "precondition keeps the native boundary in its original slot",
    );

    popup.prepend(nativeBoundary);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "native-boundary,runtime-a,runtime-b",
      "rollback preserves a Firefox boundary move while restoring managed order",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesNativeAdditionAfterManagedRemoval(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, nativeBoundary, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");
    assertEquals(
      childMarkers(popup),
      "runtime-b,native-boundary,runtime-a",
      "precondition uses the customized order",
    );

    first.remove();
    const nativeAddition = appendTestNode("menuitem");
    nativeAddition.setAttribute("data-marker", "native-addition");
    popup.append(nativeAddition);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "native-boundary,runtime-b,native-addition",
      "rollback preserves an appended Firefox item after restoring the survivor",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesNativeEdgeAdditionsAcrossOverlay(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, nativeBoundary, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");
    assertEquals(
      childMarkers(popup),
      "runtime-c,native-boundary,runtime-b,runtime-a",
      "precondition uses the customized order across a native boundary",
    );

    const firstLeadingAddition = appendTestNode("menuitem");
    firstLeadingAddition.setAttribute("data-marker", "leading-addition-1");
    const secondLeadingAddition = appendTestNode("menuitem");
    secondLeadingAddition.setAttribute("data-marker", "leading-addition-2");
    const firstTrailingAddition = appendTestNode("menuitem");
    firstTrailingAddition.setAttribute("data-marker", "trailing-addition-1");
    const secondTrailingAddition = appendTestNode("menuitem");
    secondTrailingAddition.setAttribute("data-marker", "trailing-addition-2");
    popup.prepend(firstLeadingAddition, secondLeadingAddition);
    popup.append(firstTrailingAddition, secondTrailingAddition);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "leading-addition-1,leading-addition-2,runtime-a,native-boundary,runtime-b,runtime-c,trailing-addition-1,trailing-addition-2",
      "rollback preserves Firefox addition blocks at both absolute edges",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionDistinguishesRemovalFromTailAddition(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    second.remove();
    const firstAddition = appendTestNode("menuitem");
    firstAddition.setAttribute("data-marker", "tail-addition-1");
    const secondAddition = appendTestNode("menuitem");
    secondAddition.setAttribute("data-marker", "tail-addition-2");
    popup.append(firstAddition);
    popup.append(secondAddition);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-c,tail-addition-1,tail-addition-2",
      "separate remove and append records do not masquerade as a replacement",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesMultiNodeReplacementBlock(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const leadingAddition = appendTestNode("menuitem");
    leadingAddition.setAttribute("data-marker", "leading-addition");
    popup.prepend(leadingAddition);
    const firstReplacement = appendTestNode("menuitem");
    firstReplacement.setAttribute("data-marker", "replacement-1");
    const secondReplacement = appendTestNode("menuitem");
    secondReplacement.setAttribute("data-marker", "replacement-2");
    second.replaceWith(firstReplacement, secondReplacement);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "leading-addition,runtime-a,replacement-1,replacement-2,runtime-c",
      "a prefix addition and a multi-node replacement retain distinct native placements",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesWholeNativeRebuild(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const firstNative = appendTestNode("menuitem");
    firstNative.setAttribute("data-marker", "native-rebuild-1");
    const secondNative = appendTestNode("menuitem");
    secondNative.setAttribute("data-marker", "native-rebuild-2");
    const thirdNative = appendTestNode("menuitem");
    thirdNative.setAttribute("data-marker", "native-rebuild-3");
    popup.replaceChildren(firstNative, secondNative, thirdNative);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "native-rebuild-1,native-rebuild-2,native-rebuild-3",
      "a whole Firefox rebuild establishes a new native order",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesSplitWholeNativeRebuild(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    popup.replaceChildren();
    const replacementFirst = appendTestNode("menuitem", "runtime-a");
    const replacementSecond = appendTestNode("menuitem", "runtime-b");
    const replacementThird = appendTestNode("menuitem", "runtime-c");
    popup.append(replacementThird, replacementSecond, replacementFirst);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-c,runtime-b,runtime-a",
      "a clear followed by fresh keyed nodes keeps Firefox's rebuilt order",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionRestoresAfterIdentityRebuild(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const overlayChildren = Array.from(popup.children);
    const firstInterleavedAddition = appendTestNode("menuitem");
    firstInterleavedAddition.setAttribute(
      "data-marker",
      "interleaved-addition-1",
    );
    const secondInterleavedAddition = appendTestNode("menuitem");
    secondInterleavedAddition.setAttribute(
      "data-marker",
      "interleaved-addition-2",
    );
    popup.replaceChildren(
      overlayChildren[0],
      firstInterleavedAddition,
      secondInterleavedAddition,
      ...overlayChildren.slice(1),
    );
    const nativeAddition = appendTestNode("menuitem");
    nativeAddition.setAttribute("data-marker", "post-rebuild-addition");
    popup.append(nativeAddition);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,interleaved-addition-1,interleaved-addition-2,runtime-b,runtime-c,post-rebuild-addition",
      "an identity rebuild keeps interleaved and later native additions while rolling back",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPromotesSurvivingReplacementFollower(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const removedReplacement = appendTestNode("menuitem");
    removedReplacement.setAttribute("data-marker", "removed-replacement");
    const survivingReplacement = appendTestNode("menuitem");
    survivingReplacement.setAttribute("data-marker", "surviving-replacement");
    second.replaceWith(removedReplacement, survivingReplacement);
    removedReplacement.remove();

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,surviving-replacement,runtime-c",
      "the surviving member of a replacement block inherits its native slot",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionTracksTwoRecordReplacement(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const replacement = appendTestNode("menuitem");
    replacement.setAttribute("data-marker", "two-record-replacement");
    popup.insertBefore(replacement, first);
    first.remove();

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "two-record-replacement,runtime-b",
      "insert-before plus removal is journaled as one native replacement",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesMovedBoundaryAfterManagedRemoval(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, nativeBoundary, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    first.remove();
    popup.append(nativeBoundary);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-c,native-boundary",
      "rollback preserves an explicit Firefox boundary move after a managed removal",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionAnchorsAdditionAfterNativeBoundary(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, nativeBoundary, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const nativeAddition = appendTestNode("menuitem");
    nativeAddition.setAttribute("data-marker", "boundary-addition");
    nativeBoundary.after(nativeAddition);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,native-boundary,boundary-addition,runtime-b",
      "both MutationRecord anchors preserve an addition after its native boundary",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionKeepsChainedAdditionsTogether(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const replacedAddition = appendTestNode("menuitem");
    replacedAddition.setAttribute("data-marker", "replaced-addition");
    popup.append(replacedAddition);
    const precedingAddition = appendTestNode("menuitem");
    precedingAddition.setAttribute("data-marker", "preceding-addition");
    popup.insertBefore(precedingAddition, replacedAddition);
    const finalAddition = appendTestNode("menuitem");
    finalAddition.setAttribute("data-marker", "final-addition");
    replacedAddition.replaceWith(finalAddition);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b,preceding-addition,final-addition",
      "replacement aliases keep a chained native addition block together",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesMovedManagedItem(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    popup.prepend(second);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-a,runtime-c",
      "an explicitly moved managed item remains authoritative during rollback",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionResolvesInternalManagedMoveAnchors(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    const fourth = appendTestNode("menuitem", "runtime-d");
    popup.append(first, second, third, fourth);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.d", "test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    second.after(third);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b,runtime-c,runtime-d",
      "conflicting overlay anchors resolve toward the moved item's native gap",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionContractsReplacementBlock(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const firstBlockItem = appendTestNode("menuitem");
    firstBlockItem.setAttribute("data-marker", "block-item-1");
    const secondBlockItem = appendTestNode("menuitem");
    secondBlockItem.setAttribute("data-marker", "block-item-2");
    second.replaceWith(firstBlockItem, secondBlockItem);
    const contractedItem = appendTestNode("menuitem");
    contractedItem.setAttribute("data-marker", "contracted-item");
    firstBlockItem.replaceWith(contractedItem);
    secondBlockItem.remove();

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,contracted-item,runtime-c",
      "a replacement block can contract without losing its native slot lineage",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPreservesReplacementAndOriginalRevival(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const anchoredAddition = appendTestNode("menuitem");
    anchoredAddition.setAttribute("data-marker", "anchored-addition");
    popup.insertBefore(anchoredAddition, second);
    const replacement = appendTestNode("menuitem");
    replacement.setAttribute("data-marker", "replacement");
    second.replaceWith(replacement);
    popup.append(second);

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,anchored-addition,replacement,runtime-c,runtime-b",
      "pre-replacement anchors follow the replacement while the revived original stays moved",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionTransfersMovedReplacementProvenance(): void {
  for (const sequence of ["move-then-replace", "replace-then-move"] as const) {
    const popup = appendPopup();
    const registry = new ContextMenuRegistry([createTestAdapter()]);
    try {
      const first = appendTestNode("menuitem", "runtime-a");
      const second = appendTestNode("menuitem", "runtime-b");
      const third = appendTestNode("menuitem", "runtime-c");
      popup.append(first, second, third);
      const surface = registry.resolvePopup(popup, window);
      assert(surface !== null, "test popup should resolve");

      const transaction = new ContextMenuTransaction(surface, registry, {
        order: ["test.c", "test.b", "test.a"],
        hidden: [],
      });
      assert(transaction.apply(), `${sequence}: overlay should apply`);

      const replacement = appendTestNode("menuitem");
      replacement.setAttribute("data-marker", `${sequence}-replacement`);
      if (sequence === "move-then-replace") {
        popup.append(second);
        second.replaceWith(replacement);
      } else {
        second.replaceWith(replacement);
        popup.append(replacement);
      }

      transaction.rollback();
      assertEquals(
        childMarkers(popup),
        `runtime-a,runtime-c,${sequence}-replacement`,
        `${sequence}: replacement inherits the moved native provenance`,
      );
    } finally {
      popup.remove();
    }
  }
}

function testTransactionFreezesAdditionSlotBeforeAnchorMove(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const nativeAddition = appendTestNode("menuitem");
    nativeAddition.setAttribute("data-marker", "frozen-slot-addition");
    popup.insertBefore(nativeAddition, second);
    popup.append(second);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,frozen-slot-addition,runtime-c,runtime-b",
      "a later anchor move does not change the addition's captured native slot",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionUsesMovedAnchorChronology(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    popup.append(second);
    const nativeAddition = appendTestNode("menuitem");
    nativeAddition.setAttribute("data-marker", "post-move-addition");
    popup.insertBefore(nativeAddition, second);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-c,post-move-addition,runtime-b",
      "an insertion after a move uses the remaining stable anchor's native slot",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionMapsRebuildRunsByPhysicalSlot(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.c", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const leadingAddition = appendTestNode("menuitem");
    leadingAddition.setAttribute("data-marker", "rebuild-leading");
    const firstMiddleAddition = appendTestNode("menuitem");
    firstMiddleAddition.setAttribute("data-marker", "rebuild-middle-1");
    const secondMiddleAddition = appendTestNode("menuitem");
    secondMiddleAddition.setAttribute("data-marker", "rebuild-middle-2");
    const trailingAddition = appendTestNode("menuitem");
    trailingAddition.setAttribute("data-marker", "rebuild-trailing");
    popup.replaceChildren(
      leadingAddition,
      second,
      firstMiddleAddition,
      secondMiddleAddition,
      third,
      first,
      trailingAddition,
    );

    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "rebuild-leading,runtime-a,rebuild-middle-1,rebuild-middle-2,runtime-b,runtime-c,rebuild-trailing",
      "identity-preserving rebuild runs retain their physical native gaps",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionHandlesExistingNodeReplacementAcrossBoundary(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, nativeBoundary, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    first.replaceWith(second);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-b,native-boundary,runtime-c",
      "an existing replacement occupies one native slot without duplicating its old slot",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionPrioritizesReverseExistingReplacement(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    popup.append(second, nativeBoundary, first, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.a", "test.b"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    first.replaceWith(second);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "native-boundary,runtime-b,runtime-c",
      "a replacement claim wins even when its destination follows the old slot",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionArbitratesManagedAndBoundaryReplacement(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const firstBoundary = appendTestNode("menuitem");
    firstBoundary.setAttribute("data-marker", "first-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    const secondBoundary = appendTestNode("menuitem");
    secondBoundary.setAttribute("data-marker", "second-boundary");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, firstBoundary, second, secondBoundary, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    firstBoundary.replaceWith(first);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b,second-boundary,runtime-c",
      "one live node cannot simultaneously own a managed slot and an unmanaged boundary",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionKeepsSameSubsequenceRevivalProvenance(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const replacement = appendTestNode("menuitem");
    replacement.setAttribute("data-marker", "replacement");
    second.replaceWith(replacement);
    replacement.after(second);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,replacement,runtime-b,runtime-c",
      "ordinary replacement and revival provenance survives an unchanged overlay subsequence",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionTransfersChainedReplacementOwnership(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    popup.append(second, nativeBoundary, first, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.a", "test.b"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    first.replaceWith(second);
    second.replaceWith(third);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "native-boundary,runtime-c",
      "a replacement chain transfers the destination slot carried by its live source",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionRetargetsSameReplacementOwnership(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    const nativeBoundary = appendTestNode("menuitem");
    nativeBoundary.setAttribute("data-marker", "native-boundary");
    popup.append(first, nativeBoundary, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const replacement = appendTestNode("menuitem");
    replacement.setAttribute("data-marker", "retargeted-replacement");
    first.replaceWith(replacement);
    second.replaceWith(replacement);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "native-boundary,retargeted-replacement,runtime-c",
      "moving the same replacement again transfers ownership to its latest destination",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionTransfersUnmanagedReplacementOwnership(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const firstBoundary = appendTestNode("menuitem");
    firstBoundary.setAttribute("data-marker", "first-boundary");
    const second = appendTestNode("menuitem", "runtime-b");
    const secondBoundary = appendTestNode("menuitem");
    secondBoundary.setAttribute("data-marker", "second-boundary");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, firstBoundary, second, secondBoundary, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    firstBoundary.replaceWith(secondBoundary);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,second-boundary,runtime-b,runtime-c",
      "an existing unmanaged replacement owns only its latest native boundary",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionClearsStaleAdditionLineageOnReplacement(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const replacement = appendTestNode("menuitem");
    replacement.setAttribute("data-marker", "promoted-addition");
    popup.insertBefore(replacement, first);
    third.replaceWith(replacement);
    first.remove();
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-b,promoted-addition",
      "a promoted addition cannot retain an anchor that later steals another slot",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionReclaimsIdentityAtNativeCheckpoint(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, second);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const replacement = appendTestNode("menuitem");
    replacement.setAttribute("data-marker", "checkpoint-addition");
    first.replaceWith(replacement);
    popup.replaceChildren(second, first, replacement);
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b,checkpoint-addition",
      "an identity checkpoint lets original nodes reclaim their native tokens",
    );
  } finally {
    popup.remove();
  }
}

function testTransactionKeepsOwnerlessFollowersAsAdditions(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const second = appendTestNode("menuitem", "runtime-b");
    const third = appendTestNode("menuitem", "runtime-c");
    popup.append(first, second, third);
    const surface = registry.resolvePopup(popup, window);
    assert(surface !== null, "test popup should resolve");

    const transaction = new ContextMenuTransaction(surface, registry, {
      order: ["test.c", "test.b", "test.a"],
      hidden: [],
    });
    assert(transaction.apply(), "overlay should apply");

    const removedAddition = appendTestNode("menuitem");
    removedAddition.setAttribute("data-marker", "removed-addition");
    const survivingAddition = appendTestNode("menuitem");
    survivingAddition.setAttribute("data-marker", "surviving-addition");
    popup.replaceChildren(
      third,
      second,
      first,
      removedAddition,
      survivingAddition,
    );
    removedAddition.remove();
    first.remove();
    transaction.rollback();
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-c,surviving-addition",
      "contracting an ownerless addition run preserves its checkpoint gap",
    );
  } finally {
    popup.remove();
  }
}

function testSeparatorOverlayPolicy(): void {
  const popup = appendPopup();
  try {
    const leading = appendTestNode("menuseparator");
    leading.setAttribute("data-marker", "leading");
    const first = appendTestNode("menuitem", "runtime-a");
    const middle = appendTestNode("menuseparator");
    middle.setAttribute("data-marker", "middle");
    const duplicate = appendTestNode("menuseparator");
    duplicate.setAttribute("data-marker", "duplicate");
    const hidden = appendTestNode("menuitem", "runtime-b");
    hidden.setAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE, "true");
    const trailing = appendTestNode("menuseparator");
    trailing.setAttribute("data-marker", "trailing");
    popup.append(leading, first, middle, duplicate, hidden, trailing);

    const markers = findSeparatorsToHide(popup).map((separator) =>
      separator.getAttribute("data-marker")
    ).sort().join(",");
    assertEquals(
      markers,
      "duplicate,leading,middle,trailing",
      "leading, duplicate, and trailing separators are overlaid",
    );
    assert(
      !middle.hasAttribute(FLOORP_CONTEXT_SEPARATOR_HIDDEN_ATTRIBUTE),
      "policy calculation itself does not mutate native nodes",
    );

    const collapsed = appendTestNode("menuitem");
    collapsed.setAttribute("collapsed", "true");
    assert(
      isNativelyHidden(collapsed),
      "XUL collapsed state participates in native visibility",
    );
    collapsed.setAttribute("collapsed", "false");
    assert(
      !isNativelyHidden(collapsed),
      "an explicit false collapsed attribute remains visible",
    );
  } finally {
    popup.remove();
  }
}

function testCatalogMergesNestedContainers(): void {
  const popup = appendPopup();
  const registry = new ContextMenuRegistry([createTestAdapter()]);
  const builder = new ContextMenuCatalogBuilder(registry);
  try {
    const submenu = appendTestNode("menu", "runtime-submenu");
    submenu.setAttribute("label", "Nested menu");
    const nestedPopup = appendTestNode("menupopup");
    const child = appendTestNode("menuitem", "runtime-child");
    child.setAttribute("label", "Nested command");
    nestedPopup.appendChild(child);
    submenu.appendChild(nestedPopup);
    popup.appendChild(submenu);

    const rootSurface = registry.resolvePopup(popup, window);
    const nestedSurface = registry.resolvePopup(nestedPopup, window);
    assert(rootSurface !== null, "root popup should resolve");
    assert(
      nestedSurface !== null,
      "nested popup should resolve through its root",
    );
    assertEquals(
      nestedSurface.containerKey,
      "submenu:test.submenu",
      "submenu identity becomes its child container key",
    );

    builder.record(rootSurface);
    const snapshot = builder.record(nestedSurface);
    const profile = snapshot.surfaces[0]?.profiles[0];
    assertEquals(
      profile?.containers.length,
      2,
      "recording a child merges instead of replacing the root container",
    );
    assert(
      profile?.containers.every((container) => container.complete) === true,
      "both observed container levels are marked complete",
    );
    assertEquals(
      profile?.containers.find((container) => container.key === "root")
        ?.items[0]?.childContainerKey,
      "submenu:test.submenu",
      "the root catalog links a submenu to its child container",
    );
  } finally {
    popup.remove();
  }
}

function testMenugroupIsCataloguedAndCustomizedAsContainer(): void {
  const popup = appendPopup();
  const fixture = createControllerFixture(
    createConfig(
      { order: ["test.b", "test.a"], hidden: ["test.a"] },
      "group:test.group",
    ),
  );
  try {
    const group = appendTestNode("menugroup", "runtime-group");
    const first = appendTestNode("menuitem", "runtime-a");
    const unknown = appendTestNode("menuitem");
    unknown.setAttribute("data-marker", "group-unknown");
    const second = appendTestNode("menuitem", "runtime-b");
    group.append(first, unknown, second);
    popup.appendChild(group);

    fixture.controller.attach();
    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    runPopupShowingReconcile(fixture.callbacks);
    assertEquals(
      childMarkers(group),
      "runtime-b,group-unknown,runtime-a",
      "a menugroup container receives its own order overlay",
    );
    assert(
      first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "a menugroup child receives its own visibility overlay",
    );

    const profile = fixture.reporter.reports.at(-1)?.surfaces[0]?.profiles[0];
    assertEquals(
      profile?.containers.length,
      2,
      "root catalog capture includes the virtual group container",
    );
    assertEquals(
      profile?.containers.find((container) => container.key === "root")
        ?.items[0]?.childContainerKey,
      "group:test.group",
      "group descriptor links to its virtual child container",
    );

    popup.dispatchEvent(new Event("popuphiding", { bubbles: true }));
    assertEquals(
      childMarkers(group),
      "runtime-a,group-unknown,runtime-b",
      "root popup hiding rolls the group transaction back",
    );
  } finally {
    fixture.controller.destroy();
    popup.remove();
  }
}

function testControllerAppliesAfterNativeBuilderAndRollsBack(): void {
  const popup = appendPopup();
  const fixture = createControllerFixture(
    createConfig({ order: ["test.b", "test.a"], hidden: ["test.a"] }),
  );
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const unknown = appendTestNode("menuitem");
    unknown.setAttribute("data-marker", "unknown");
    popup.append(first, unknown);
    popup.addEventListener("popupshowing", () => {
      popup.appendChild(appendTestNode("menuitem", "runtime-b"));
    }, { once: true });

    fixture.controller.attach();
    popup.dispatchEvent(
      new Event("popupshowing", { bubbles: true, cancelable: true }),
    );
    assertEquals(
      childMarkers(popup),
      "runtime-a,unknown,runtime-b",
      "native target listener runs before the scheduled overlay",
    );
    assert(
      !first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "capture listener does not mutate synchronously",
    );

    runPopupShowingReconcile(fixture.callbacks);
    assertEquals(
      childMarkers(popup),
      "runtime-b,unknown,runtime-a",
      "microtask applies to the native builder's final children",
    );
    assert(
      first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "microtask applies visibility after native popupshowing",
    );
    assertEquals(
      fixture.reporter.reports.at(-1)?.surfaces[0]?.profiles[0]
        ?.containers[0]?.items.length,
      3,
      "catalog observes the native popup before customization",
    );

    const late = appendTestNode("menuitem");
    late.setAttribute("data-marker", "late-native-item");
    popup.appendChild(late);
    popup.dispatchEvent(new Event("popupshown", { bubbles: true }));
    runNextMicrotask(fixture.callbacks);
    assertEquals(
      childMarkers(popup),
      "runtime-b,unknown,runtime-a,late-native-item",
      "popupshown reconciles asynchronous native builder mutations",
    );
    assertEquals(
      fixture.reporter.reports.at(-1)?.surfaces[0]?.profiles[0]
        ?.containers[0]?.items.length,
      4,
      "popupshown refreshes the catalog after asynchronous builders",
    );

    popup.dispatchEvent(new Event("popuphiding", { bubbles: true }));
    assertEquals(
      childMarkers(popup),
      "runtime-a,unknown,runtime-b,late-native-item",
      "popuphiding restores the native order",
    );
    assert(
      !first.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "popuphiding removes visibility overlays",
    );
  } finally {
    fixture.controller.destroy();
    popup.remove();
  }
}

async function testControllerWaitsForNativeBuilderMicrotask(): Promise<void> {
  const popup = appendPopup();
  const reporter = new RecordingReporter();
  const controller = new ContextMenuController({
    window,
    registry: new ContextMenuRegistry([createTestAdapter()]),
    configStore: new ContextMenuConfigStore(
      new FakePreferenceSource(
        createConfig({ order: ["test.b", "test.a"] }),
      ),
    ),
    catalogReporter: reporter,
    ownerId: "async-builder-test-window",
  });
  try {
    const first = appendTestNode("menuitem", "runtime-a");
    const nativeAnchor = appendTestNode("menuitem");
    nativeAnchor.setAttribute("data-marker", "native-anchor");
    const second = appendTestNode("menuitem", "runtime-b");
    popup.append(first, nativeAnchor, second);

    let builderInput = "";
    popup.addEventListener("popupshowing", () => {
      globalThis.queueMicrotask(() => {
        globalThis.queueMicrotask(() => {
          builderInput = childMarkers(popup);
          const asyncItem = appendTestNode("menuitem");
          asyncItem.setAttribute("data-marker", "async-native-item");
          popup.insertBefore(asyncItem, first);
        });
      });
    }, { once: true });

    controller.attach();
    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    await Promise.resolve();
    await Promise.resolve();

    assertEquals(
      builderInput,
      "runtime-a,native-anchor,runtime-b",
      "nested native builder microtasks read the uncustomized popup",
    );
    assertEquals(
      childMarkers(popup),
      "async-native-item,runtime-a,native-anchor,runtime-b",
      "the native builder finishes before the customization is applied",
    );

    await nextAnimationFrame();
    assertEquals(
      childMarkers(popup),
      "async-native-item,runtime-b,native-anchor,runtime-a",
      "the deferred pass customizes the builder's final native DOM",
    );

    popup.dispatchEvent(new Event("popuphiding", { bubbles: true }));
    assertEquals(
      childMarkers(popup),
      "async-native-item,runtime-a,native-anchor,runtime-b",
      "rollback preserves the async builder's native insertion",
    );
  } finally {
    controller.destroy();
    popup.remove();
  }
}

async function testControllerDefaultSchedulerUsesWindowReceiver(): Promise<
  void
> {
  const popup = appendPopup();
  const reporter = new RecordingReporter();
  const controller = new ContextMenuController({
    window,
    registry: new ContextMenuRegistry([createTestAdapter()]),
    configStore: new ContextMenuConfigStore(
      new FakePreferenceSource(DEFAULT_CONTEXT_MENU_CONFIG),
    ),
    catalogReporter: reporter,
    ownerId: "default-scheduler-test-window",
  });
  try {
    popup.appendChild(appendTestNode("menuitem", "runtime-a"));
    controller.attach();
    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    await nextAnimationFrame();

    const root = reporter.reports.at(-1)?.surfaces.find((surface) =>
      surface.key === TEST_SURFACE_KEY
    )?.profiles[0].containers.find((container) => container.key === "root");
    assertEquals(
      root?.complete,
      true,
      "the default scheduler reaches reconciliation in a real Window realm",
    );
    assertEquals(
      root?.items[0]?.key,
      "test.a",
      "the default scheduler records popup children instead of leaving a placeholder",
    );
  } finally {
    controller.destroy();
    popup.remove();
  }
}

function testControllerUnknownKeysAndEmptyConfigAreDomNoOps(): void {
  for (
    const config of [
      DEFAULT_CONTEXT_MENU_CONFIG,
      createConfig({ order: ["missing-b", "missing-a"], hidden: ["missing"] }),
    ]
  ) {
    const popup = appendPopup();
    const fixture = createControllerFixture(config);
    try {
      popup.append(
        appendTestNode("menuitem", "runtime-a"),
        appendTestNode("menuitem", "runtime-b"),
      );
      const before = popup.innerHTML;
      fixture.controller.attach();
      popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
      runPopupShowingReconcile(fixture.callbacks);
      assertEquals(
        popup.innerHTML,
        before,
        "empty or unknown-only configuration leaves DOM unchanged",
      );
      assertEquals(
        document.querySelector("[data-floorp-context-menu-style]"),
        null,
        "a no-op never injects overlay CSS",
      );
    } finally {
      fixture.controller.destroy();
      popup.remove();
    }
  }
}

async function testControllerObservesNativeMutationsWithoutOverlayLoop(): Promise<
  void
> {
  const popup = appendPopup();
  const fixture = createControllerFixture(
    createConfig({ order: ["test.b", "test.a"] }),
  );
  try {
    popup.append(
      appendTestNode("menuitem", "runtime-a"),
      appendTestNode("menuitem", "runtime-b"),
    );
    fixture.controller.attach();
    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    runPopupShowingReconcile(fixture.callbacks);
    await flushMutationObservers();
    assertEquals(
      fixture.callbacks.length,
      0,
      "Floorp's own reorder does not feed the native mutation observer",
    );

    const late = appendTestNode("menuitem");
    late.setAttribute("data-marker", "observed-native-item");
    popup.appendChild(late);
    await flushMutationObservers();
    assertEquals(
      fixture.callbacks.length,
      1,
      "an open popup schedules reconciliation after a native child mutation",
    );
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b,observed-native-item",
      "observer rolls the old overlay back before reconciling",
    );
    runNextMicrotask(fixture.callbacks);
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-a,observed-native-item",
      "observed native children are merged into the refreshed overlay",
    );
    await flushMutationObservers();
    assertEquals(
      fixture.callbacks.length,
      0,
      "observer remains quiet after refreshed Floorp mutations",
    );

    late.setAttribute("aria-label", "Updated native label");
    await flushMutationObservers();
    assertEquals(
      fixture.callbacks.length,
      1,
      "a native aria-label change schedules catalog reconciliation",
    );
    runNextMicrotask(fixture.callbacks);
    const reportedItems = fixture.reporter.reports.at(-1)?.surfaces
      .flatMap((surface) => surface.profiles)
      .flatMap((profile) => profile.containers)
      .flatMap((container) => container.items) ?? [];
    assert(
      reportedItems.some((item) => item.label === "Updated native label"),
      "the refreshed catalog exposes the updated accessible label",
    );
  } finally {
    fixture.controller.destroy();
    popup.remove();
  }
}

async function testCancelledPopupHidingRestoresOverlayAndObserver(): Promise<
  void
> {
  const popup = appendPopup();
  const fixture = createControllerFixture(
    createConfig({ order: ["test.b", "test.a"] }),
  );
  try {
    popup.append(
      appendTestNode("menuitem", "runtime-a"),
      appendTestNode("menuitem", "runtime-b"),
    );
    fixture.controller.attach();
    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    runPopupShowingReconcile(fixture.callbacks);
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-a",
      "the initial overlay is active",
    );

    popup.addEventListener("popuphiding", (event) => event.preventDefault(), {
      once: true,
    });
    const hiding = new Event("popuphiding", {
      bubbles: true,
      cancelable: true,
    });
    assertEquals(
      popup.dispatchEvent(hiding),
      false,
      "the native close is cancelled",
    );
    assertEquals(
      childMarkers(popup),
      "runtime-a,runtime-b",
      "capture phase exposes native order to popuphiding handlers",
    );

    runNextMicrotask(fixture.callbacks);
    runNextMicrotask(fixture.callbacks);
    assertEquals(
      childMarkers(popup),
      "runtime-b,runtime-a",
      "a cancelled close restores the customization",
    );

    popup.appendChild(appendTestNode("menuitem"));
    await flushMutationObservers();
    assertEquals(
      fixture.callbacks.length,
      1,
      "a cancelled close also restores native mutation monitoring",
    );
    runNextMicrotask(fixture.callbacks);
  } finally {
    fixture.controller.destroy();
    popup.remove();
  }
}

async function testRootObserverExcludesNestedPopupBoundary(): Promise<void> {
  const popup = appendPopup();
  const fixture = createControllerFixture(DEFAULT_CONTEXT_MENU_CONFIG);
  try {
    const submenu = appendTestNode("menu", "runtime-submenu");
    const nestedPopup = appendTestNode("menupopup");
    nestedPopup.appendChild(appendTestNode("menuitem", "runtime-child"));
    submenu.appendChild(nestedPopup);
    popup.appendChild(submenu);

    fixture.controller.attach();
    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    runPopupShowingReconcile(fixture.callbacks);
    nestedPopup.appendChild(appendTestNode("menuitem"));
    await flushMutationObservers();
    assertEquals(
      fixture.callbacks.length,
      0,
      "root observer ignores mutations inside a closed child menupopup",
    );

    nestedPopup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    runPopupShowingReconcile(fixture.callbacks);
    nestedPopup.appendChild(appendTestNode("menuitem"));
    await flushMutationObservers();
    assertEquals(
      fixture.callbacks.length,
      1,
      "an opened child menupopup receives its own scoped observer",
    );
    runNextMicrotask(fixture.callbacks);
  } finally {
    fixture.controller.destroy();
    popup.remove();
  }
}

const tests: TestCase[] = [
  {
    name: "config parser and dormant profile semantics",
    fn: testConfigParsingAndDormantProfiles,
  },
  {
    name: "config store defaults enabled",
    fn: testConfigStoreDefaultsEnabled,
  },
  {
    name: "controller catalog owner IDs are unique",
    fn: testControllerOwnerIdsAreProcessUnique,
  },
  {
    name: "native-slot merge preserves unknown nodes",
    fn: testNativeSlotMergeAndSameParentGuard,
  },
  {
    name: "page menus excluded and extensions catalogued read-only",
    fn: testRegistryExcludesPageItemsAndKeepsExtensionsReadOnly,
  },
  {
    name: "explicit aliases override broad readonly fallbacks",
    fn: testExplicitAliasesOverrideBroadReadonlyFallbacks,
  },
  {
    name: "catalog uses localized accessible labels",
    fn: testCatalogUsesLocalizedAccessibleLabels,
  },
  {
    name: "duplicate stable keys are protected in the catalog",
    fn: testCatalogProtectsDuplicateStableKeys,
  },
  {
    name: "generic browser context popup fallback",
    fn: testRegistryGenericBrowserContextFallback,
  },
  {
    name: "known Firefox popups have document-scoped surfaces",
    fn: testKnownFirefoxPopupSurfacesAreDocumentScoped,
  },
  {
    name: "Firefox popup clone selectors remain narrow",
    fn: testFirefoxPopupCloneSelectorsStayNarrow,
  },
  {
    name: "current Firefox context menu contracts",
    fn: testCurrentFirefoxContextMenuContracts,
  },
  {
    name: "transaction overlays and rollback",
    fn: testTransactionUsesOverlayAndRollsBack,
  },
  {
    name: "transaction rollback isolates per-parent restore failures",
    fn: testTransactionRollbackIsolatesRestoreFailures,
  },
  {
    name: "transaction moves commands across known separators",
    fn: testTransactionMovesAcrossKnownSeparator,
  },
  {
    name: "transaction preserves protected native slots",
    fn: testTransactionKeepsProtectedItemInNativeSlot,
  },
  {
    name: "transaction restores survivors after native node changes",
    fn: testTransactionRestoresSurvivorsAfterNativeNodeChanges,
  },
  {
    name: "transaction restores a single survivor to its native slot",
    fn: testTransactionRestoresSingleSurvivorToNativeSlot,
  },
  {
    name: "transaction restores survivors across native regions",
    fn: testTransactionRestoresSurvivorsAcrossNativeRegions,
  },
  {
    name: "transaction restores after native boundary changes",
    fn: testTransactionRestoresAfterNativeBoundaryChanges,
  },
  {
    name: "transaction restores after combined native changes",
    fn: testTransactionRestoresAfterCombinedNativeChanges,
  },
  {
    name: "transaction preserves native boundary moves",
    fn: testTransactionPreservesNativeBoundaryMove,
  },
  {
    name: "transaction preserves native additions after managed removal",
    fn: testTransactionPreservesNativeAdditionAfterManagedRemoval,
  },
  {
    name: "transaction preserves native edge additions across an overlay",
    fn: testTransactionPreservesNativeEdgeAdditionsAcrossOverlay,
  },
  {
    name: "transaction distinguishes removal from tail additions",
    fn: testTransactionDistinguishesRemovalFromTailAddition,
  },
  {
    name: "transaction preserves a multi-node replacement block",
    fn: testTransactionPreservesMultiNodeReplacementBlock,
  },
  {
    name: "transaction preserves a whole native rebuild",
    fn: testTransactionPreservesWholeNativeRebuild,
  },
  {
    name: "transaction preserves a split whole native rebuild",
    fn: testTransactionPreservesSplitWholeNativeRebuild,
  },
  {
    name: "transaction restores after an identity rebuild",
    fn: testTransactionRestoresAfterIdentityRebuild,
  },
  {
    name: "transaction promotes a surviving replacement follower",
    fn: testTransactionPromotesSurvivingReplacementFollower,
  },
  {
    name: "transaction tracks a two-record replacement",
    fn: testTransactionTracksTwoRecordReplacement,
  },
  {
    name: "transaction preserves a moved boundary after managed removal",
    fn: testTransactionPreservesMovedBoundaryAfterManagedRemoval,
  },
  {
    name: "transaction anchors an addition after a native boundary",
    fn: testTransactionAnchorsAdditionAfterNativeBoundary,
  },
  {
    name: "transaction keeps chained additions together",
    fn: testTransactionKeepsChainedAdditionsTogether,
  },
  {
    name: "transaction preserves an explicitly moved managed item",
    fn: testTransactionPreservesMovedManagedItem,
  },
  {
    name: "transaction resolves internal managed-move anchors",
    fn: testTransactionResolvesInternalManagedMoveAnchors,
  },
  {
    name: "transaction contracts a replacement block",
    fn: testTransactionContractsReplacementBlock,
  },
  {
    name: "transaction preserves replacement and original revival",
    fn: testTransactionPreservesReplacementAndOriginalRevival,
  },
  {
    name: "transaction transfers moved replacement provenance",
    fn: testTransactionTransfersMovedReplacementProvenance,
  },
  {
    name: "transaction freezes an addition slot before an anchor move",
    fn: testTransactionFreezesAdditionSlotBeforeAnchorMove,
  },
  {
    name: "transaction uses moved-anchor chronology",
    fn: testTransactionUsesMovedAnchorChronology,
  },
  {
    name: "transaction maps rebuild runs by physical slot",
    fn: testTransactionMapsRebuildRunsByPhysicalSlot,
  },
  {
    name: "transaction handles existing-node replacement across a boundary",
    fn: testTransactionHandlesExistingNodeReplacementAcrossBoundary,
  },
  {
    name: "transaction prioritizes reverse existing-node replacement",
    fn: testTransactionPrioritizesReverseExistingReplacement,
  },
  {
    name: "transaction arbitrates managed and boundary replacement",
    fn: testTransactionArbitratesManagedAndBoundaryReplacement,
  },
  {
    name: "transaction keeps same-subsequence revival provenance",
    fn: testTransactionKeepsSameSubsequenceRevivalProvenance,
  },
  {
    name: "transaction transfers chained replacement ownership",
    fn: testTransactionTransfersChainedReplacementOwnership,
  },
  {
    name: "transaction retargets the same replacement ownership",
    fn: testTransactionRetargetsSameReplacementOwnership,
  },
  {
    name: "transaction transfers unmanaged replacement ownership",
    fn: testTransactionTransfersUnmanagedReplacementOwnership,
  },
  {
    name: "transaction clears stale addition lineage on replacement",
    fn: testTransactionClearsStaleAdditionLineageOnReplacement,
  },
  {
    name: "transaction reclaims identity at a native checkpoint",
    fn: testTransactionReclaimsIdentityAtNativeCheckpoint,
  },
  {
    name: "transaction keeps ownerless followers as additions",
    fn: testTransactionKeepsOwnerlessFollowersAsAdditions,
  },
  {
    name: "separator overlay policy",
    fn: testSeparatorOverlayPolicy,
  },
  {
    name: "catalog merges nested containers",
    fn: testCatalogMergesNestedContainers,
  },
  {
    name: "menugroup is a virtual editable container",
    fn: testMenugroupIsCataloguedAndCustomizedAsContainer,
  },
  {
    name: "controller defers apply and rolls back",
    fn: testControllerAppliesAfterNativeBuilderAndRollsBack,
  },
  {
    name: "controller seeds an initial popup without claiming completeness",
    fn: testControllerSeedsInitialPopupWithoutClaimingComplete,
  },
  {
    name: "catalog seed does not regress to an empty clone",
    fn: testCatalogSeedDoesNotRegressToEmptyClone,
  },
  {
    name: "controller waits for native builder microtasks",
    fn: testControllerWaitsForNativeBuilderMicrotask,
  },
  {
    name: "controller default scheduler keeps the Window receiver",
    fn: testControllerDefaultSchedulerUsesWindowReceiver,
  },
  {
    name: "empty and unknown config are DOM no-ops",
    fn: testControllerUnknownKeysAndEmptyConfigAreDomNoOps,
  },
  {
    name: "native mutation observer reconciles without overlay loop",
    fn: testControllerObservesNativeMutationsWithoutOverlayLoop,
  },
  {
    name: "cancelled popuphiding restores overlay and observer",
    fn: testCancelledPopupHidingRestoresOverlayAndObserver,
  },
  {
    name: "root observer excludes nested popup boundary",
    fn: testRootObserverExcludesNestedPopupBoundary,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("contextMenuRuntime.test.ts", tests);
}
