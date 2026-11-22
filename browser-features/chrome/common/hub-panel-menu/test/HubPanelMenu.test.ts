// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type {
  AppMenuAdapterConnection,
  AppMenuOwner,
} from "../../../../modules/modules/feature-host/AppMenuRegistry.sys.mts";
import {
  HUB_MENU_LABEL_FALLBACK,
  mountOwnedHubPanelMenuView,
  observeHubMenuReadiness,
} from "../HubPanelMenuView.tsx";
import type {
  HubPanelMenuFocusContext,
  OwnedHubPanelMenuView,
} from "../types.ts";

type RegistryModule =
  typeof import("../../../../modules/modules/feature-host/AppMenuRegistry.sys.mts");
const { AppMenuRegistry, HUB_PANEL_MENU_FEATURE_ID } = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/feature-host/AppMenuRegistry.sys.mjs",
  ) as RegistryModule;
type AppMenuRegistryInstance = InstanceType<typeof AppMenuRegistry>;

const OWNER: AppMenuOwner = Object.freeze({
  featureId: HUB_PANEL_MENU_FEATURE_ID,
  hostEpoch: "owned-view-test",
  generation: 1,
});

interface MenuFixture {
  readonly container: HTMLDivElement;
  readonly parent: Element;
  readonly settings: Element;
  readonly marker: Element;
}

function createMenuFixture(): MenuFixture {
  const container = document.createElement("div");
  const parent = document.createXULElement("vbox");
  const settings = document.createXULElement("toolbarbutton");
  const marker = document.createXULElement("toolbarbutton");
  parent.append(settings, marker);
  container.append(parent);
  return { container, parent, settings, marker };
}

function mountView(
  fixture: MenuFixture,
  options: {
    readonly onCommand?: () => void;
    readonly readLabel?: (fallback: string) => unknown;
    readonly onFailure?: (owner: AppMenuOwner, error: Error) => void;
    readonly buttonId?: string;
    readonly enabled?: boolean;
    readonly focusContext?: HubPanelMenuFocusContext;
  } = {},
): {
  readonly view: OwnedHubPanelMenuView;
  readonly translationChanged: () => void;
} {
  let translationChanged = () => {};
  const view = mountOwnedHubPanelMenuView({
    parent: fixture.parent,
    marker: fixture.settings.nextElementSibling,
    buttonId: options.buttonId ?? "appMenu-floorp-hub-button",
    localizationKey: "hub.menu.title",
    enabled: options.enabled ?? true,
    owner: OWNER,
    onCommand: options.onCommand ?? (() => {}),
    readLabel: (_key, fallback) =>
      options.readLabel ? options.readLabel(fallback) : fallback,
    observeTranslations: (update) => translationChanged = update,
    reportFailure: options.onFailure,
    focusContext: options.focusContext,
  });
  return { view, translationChanged: () => translationChanged() };
}

function testOwnedViewUsesItsExactElementAndSettingsSlot(): void {
  const fixture = createMenuFixture();
  let view: OwnedHubPanelMenuView | null = null;
  try {
    view = mountView(fixture).view;
    assertEquals(view.node.id, "appMenu-floorp-hub-button", "fixed button ID");
    assertEquals(
      view.node.getAttribute("class"),
      "subviewbutton",
      "legacy menu class is preserved",
    );
    assertEquals(
      view.node.getAttribute("tabindex"),
      "-1",
      "owned HUB button is programmatically focusable",
    );
    assertEquals(
      view.node.getAttribute("label"),
      HUB_MENU_LABEL_FALLBACK,
      "missing translation uses the default label",
    );
    assert(
      fixture.settings.nextElementSibling === view.node &&
        view.node.nextElementSibling === fixture.marker,
      "the owned button is inserted immediately after Settings",
    );
    view.setEnabled(false);
    assert(view.node.disabled, "XUL disabled property updates");
    assert(view.node.hasAttribute("disabled"), "disabled state updates");
    view.setEnabled(true);
    assert(!view.node.disabled, "XUL disabled property clears");
    assert(!view.node.hasAttribute("disabled"), "enabled state updates");
  } finally {
    view?.dispose();
    fixture.container.remove();
  }
}

function testFocusClaimRestoresOnlyWithoutUserMovement(): void {
  const fixture = createMenuFixture();
  const trigger = document.createXULElement("toolbarbutton");
  const other = document.createElement("button");
  const priorActive = document.activeElement;
  const testWindow = document.defaultView;
  if (!testWindow) throw new Error("focus test requires a window");
  const settings = fixture.settings as Element & { focus: () => void };
  fixture.container.style.position = "fixed";
  fixture.container.style.left = "0";
  fixture.container.style.top = "0";
  fixture.container.style.zIndex = "2147483647";
  fixture.container.style.minWidth = "12rem";
  fixture.container.style.minHeight = "4rem";
  fixture.parent.setAttribute("style", "min-width: 12rem; min-height: 4rem;");
  fixture.settings.setAttribute("label", "Settings");
  fixture.settings.setAttribute("tabindex", "0");
  fixture.marker.setAttribute("label", "marker");
  trigger.setAttribute("label", "Menu");
  trigger.setAttribute("open", "true");
  trigger.setAttribute("tabindex", "0");
  const focusContext: HubPanelMenuFocusContext = {
    trigger,
    settings: fixture.settings,
    isMenuOpen: () => trigger.getAttribute("open") === "true",
    isForeground: () => true,
  };
  let oldView: OwnedHubPanelMenuView | null = null;
  let replacement: OwnedHubPanelMenuView | null = null;
  let movedView: OwnedHubPanelMenuView | null = null;
  try {
    fixture.container.append(trigger, other);
    document.body.append(fixture.container);
    oldView = mountView(fixture, {
      buttonId: "stage2-owned-focus-old",
      focusContext,
    }).view;
    assert(
      oldView.node.isConnected && oldView.node.getClientRects().length > 0,
      "old view fixture is connected and visible",
    );
    oldView.node.focus();
    assert(document.activeElement === oldView.node, "old view receives focus");
    let descendantBlurObserved = false;
    const observeDescendantBlur = (event: FocusEvent): void => {
      if (oldView && event.composedPath().includes(oldView.node)) {
        descendantBlurObserved = true;
      }
    };
    testWindow.addEventListener("blur", observeDescendantBlur, true);
    try {
      oldView.dispose("replacement");
    } finally {
      testWindow.removeEventListener("blur", observeDescendantBlur, true);
    }
    assert(
      descendantBlurObserved,
      "old view blur reaches the window capture listener during handoff",
    );
    assert(
      document.activeElement === fixture.settings,
      "focused removal uses visible Settings as fallback",
    );
    replacement = mountView(fixture, {
      buttonId: "stage2-owned-focus-new",
      enabled: false,
      focusContext,
    }).view;
    assert(
      document.activeElement === settings,
      "disabled replacement does not consume the claim",
    );
    replacement.setEnabled(true);
    assert(
      document.activeElement === replacement.node,
      "replacement consumes a valid focus claim",
    );
    replacement.dispose("replacement");
    other.focus();
    settings.focus();
    movedView = mountView(fixture, {
      buttonId: "stage2-owned-focus-after-movement",
      focusContext,
    }).view;
    assert(
      document.activeElement === fixture.settings,
      "moving away and back cancels restoration",
    );
    movedView.setEnabled(false);
    movedView.setEnabled(true);
    assert(
      document.activeElement === settings,
      "disable and re-enable do not steal focus after a canceled claim",
    );
  } finally {
    movedView?.dispose();
    oldView?.dispose();
    replacement?.dispose();
    fixture.container.remove();
    if (priorActive && "focus" in priorActive) {
      (priorActive as Element & { focus: () => void }).focus();
    }
  }
}

function testDelayedOldCleanupPreservesReplacementNodeAndHandler(): void {
  const fixture = createMenuFixture();
  let oldCommands = 0;
  let replacementCommands = 0;
  let oldView: OwnedHubPanelMenuView | null = null;
  let replacement: OwnedHubPanelMenuView | null = null;
  try {
    oldView = mountView(fixture, { onCommand: () => oldCommands++ }).view;
    const oldNode = oldView.node;
    replacement = mountView(fixture, {
      onCommand: () => replacementCommands++,
    }).view;

    oldView.dispose();
    oldNode.dispatchEvent(new Event("command"));
    replacement.node.dispatchEvent(new Event("command"));

    assert(!fixture.parent.contains(oldNode), "old cleanup removes its node");
    assert(
      fixture.parent.contains(replacement.node),
      "old cleanup preserves the replacement node with the same ID",
    );
    assertEquals(oldCommands, 0, "disposed old handler no longer fires");
    assertEquals(
      replacementCommands,
      1,
      "replacement handler remains active after old cleanup",
    );
  } finally {
    oldView?.dispose();
    replacement?.dispose();
    fixture.container.remove();
  }
}

function testInvalidInitialLabelsRejectAndCleanSetup(): void {
  for (const invalidLabel of [17, "   "]) {
    const fixture = createMenuFixture();
    let view: OwnedHubPanelMenuView | null = null;
    try {
      let thrown = false;
      try {
        view = mountView(fixture, {
          readLabel: () => invalidLabel,
        }).view;
      } catch (error) {
        thrown = error instanceof Error &&
          error.message === "invalid HUB menu label";
      }
      assert(thrown, "invalid initial translation rejects render");
      assertEquals(
        fixture.parent.children.length,
        2,
        "failed initial setup leaves only owned Settings and marker nodes",
      );
    } finally {
      view?.dispose();
      fixture.container.remove();
    }
  }
}

function testLateTranslationFailureReportsThroughOwnedView(): void {
  const fixture = createMenuFixture();
  let label: unknown = "Initial label";
  const failures: { owner: AppMenuOwner; error: Error }[] = [];
  let view: OwnedHubPanelMenuView | null = null;
  let translationChanged = () => {};
  try {
    const mounted = mountView(fixture, {
      readLabel: () => label,
      onFailure: (owner, error) => failures.push({ owner, error }),
    });
    view = mounted.view;
    translationChanged = mounted.translationChanged;
    label = null;
    translationChanged();
    assertEquals(failures.length, 1, "late invalid translation is reported");
    assertEquals(
      JSON.stringify(failures[0].owner),
      JSON.stringify(OWNER),
      "translation failure is tagged with its view owner",
    );
    assertEquals(
      failures[0].error.message,
      "invalid HUB menu label",
      "translation failure preserves the validation error",
    );
    assertEquals(
      view.node.getAttribute("label"),
      "Initial label",
      "invalid late label does not replace the last valid label",
    );
  } finally {
    view?.dispose();
    fixture.container.remove();
  }
}

function testTranslationCleanupFailureStillRemovesOwnedResources(): void {
  const fixture = createMenuFixture();
  let commands = 0;
  let view: OwnedHubPanelMenuView | null = null;
  try {
    view = mountOwnedHubPanelMenuView({
      parent: fixture.parent,
      marker: fixture.settings.nextElementSibling,
      buttonId: "stage2-cleanup-failure",
      localizationKey: "hub.menu.title",
      enabled: true,
      owner: OWNER,
      onCommand: () => commands++,
      readLabel: (_key, fallback) => fallback,
      observeTranslations: () => () => {
        throw new Error("translation cleanup failed");
      },
    });
    let cleanupFailure: unknown;
    try {
      view.dispose();
    } catch (error) {
      cleanupFailure = error;
    }
    assert(
      cleanupFailure instanceof AggregateError,
      "translation cleanup errors are aggregated",
    );
    assertEquals(
      fixture.parent.children.length,
      2,
      "translation cleanup failure still removes the owned node",
    );
    view.node.dispatchEvent(new Event("command"));
    assertEquals(
      commands,
      0,
      "translation cleanup failure removes the listener",
    );
    view.dispose();
  } finally {
    view?.dispose();
    fixture.container.remove();
  }
}

function testFocusTransferFailureStillRemovesOwnedResources(): void {
  const fixture = createMenuFixture();
  const trigger = document.createXULElement("toolbarbutton");
  const priorActive = document.activeElement;
  fixture.container.style.position = "fixed";
  fixture.container.style.left = "0";
  fixture.container.style.top = "0";
  fixture.container.style.zIndex = "2147483647";
  fixture.container.style.minWidth = "12rem";
  fixture.container.style.minHeight = "4rem";
  fixture.parent.setAttribute("style", "min-width: 12rem; min-height: 4rem;");
  fixture.settings.setAttribute("label", "Settings");
  fixture.settings.setAttribute("tabindex", "0");
  trigger.setAttribute("open", "true");
  const focusContext: HubPanelMenuFocusContext = {
    trigger,
    settings: fixture.settings,
    isMenuOpen: () => true,
    isForeground: () => true,
  };
  Object.defineProperty(fixture.settings, "focus", {
    configurable: true,
    value: () => {
      throw new Error("focus transfer failed");
    },
  });
  let commands = 0;
  let translationDisposals = 0;
  let view: OwnedHubPanelMenuView | null = null;
  try {
    fixture.container.append(trigger);
    document.body.append(fixture.container);
    view = mountOwnedHubPanelMenuView({
      parent: fixture.parent,
      marker: fixture.settings.nextElementSibling,
      buttonId: "stage2-focus-cleanup-failure",
      localizationKey: "hub.menu.title",
      enabled: true,
      owner: OWNER,
      onCommand: () => commands++,
      readLabel: (_key, fallback) => fallback,
      observeTranslations: () => () => translationDisposals++,
      focusContext,
    });
    view.node.setAttribute("tabindex", "0");
    view.node.focus();
    assert(document.activeElement === view.node, "owned view receives focus");

    let cleanupFailure: unknown;
    try {
      view.dispose("replacement");
    } catch (error) {
      cleanupFailure = error;
    }
    assert(
      cleanupFailure instanceof AggregateError,
      "focus transfer error is aggregated with cleanup",
    );
    assert(
      cleanupFailure.errors.some((error) =>
        error instanceof Error && error.message === "focus transfer failed"
      ),
      "focus transfer error is retained",
    );
    assert(
      !fixture.parent.contains(view.node),
      "focus transfer error still removes the owned node",
    );
    view.node.dispatchEvent(new Event("command"));
    assertEquals(commands, 0, "focus transfer error removes the listener");
    assertEquals(
      translationDisposals,
      1,
      "focus transfer error still disposes translation observation",
    );
  } finally {
    view?.dispose();
    fixture.container.remove();
    if (priorActive && "focus" in priorActive) {
      (priorActive as Element & { focus: () => void }).focus();
    }
  }
}

async function testReadinessObserverLifetimeIsBounded(): Promise<void> {
  const fixture = createMenuFixture();
  const trigger = document.createElement("button");
  let notifyCount = 0;
  let ready = false;
  let stop = () => {};
  try {
    stop = observeHubMenuReadiness(
      fixture.container,
      () => ready,
      () => notifyCount++,
    );
    fixture.container.append(trigger);
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    assertEquals(notifyCount, 0, "unready menu mutations do not notify");
    ready = true;
    trigger.setAttribute("open", "true");
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    assertEquals(notifyCount, 1, "ready menu mutation notifies once");
    trigger.removeAttribute("open");
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    assertEquals(notifyCount, 1, "observer disconnects after readiness");
  } finally {
    stop();
    fixture.container.remove();
  }

  const disposedFixture = createMenuFixture();
  let disposedNotifications = 0;
  let stopWaiting = () => {};
  try {
    stopWaiting = observeHubMenuReadiness(
      disposedFixture.container,
      () => true,
      () => disposedNotifications++,
    );
    stopWaiting();
    disposedFixture.container.append(document.createElement("span"));
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    assertEquals(
      disposedNotifications,
      0,
      "disposed readiness observer ignores later DOM changes",
    );
  } finally {
    stopWaiting();
    disposedFixture.container.remove();
  }
}

async function testInitialMountTranslationFailureFencesReadiness(): Promise<
  void
> {
  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser") as
    | Window
    | null;
  if (!browserWindow) throw new Error("test requires a browser window");
  const trigger = browserWindow.document.getElementById("PanelUI-menu-button");
  if (!trigger) throw new Error("test browser menu trigger is unavailable");
  const fixture = createMenuFixture();
  const registry = new AppMenuRegistry();
  let claim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null = null;
  let connection: AppMenuAdapterConnection | null = null;
  const viewRef: { current: OwnedHubPanelMenuView | null } = { current: null };
  let translationChanged = () => {};
  let reportAccepted = false;
  let hostFailures = 0;
  try {
    claim = registry.claimHost(HUB_PANEL_MENU_FEATURE_ID, "owned-view-race");
    connection = registry.registerAdapter(browserWindow, {
      render: (request) => {
        let reads = 0;
        const mounted = mountOwnedHubPanelMenuView({
          parent: fixture.parent,
          marker: fixture.settings.nextElementSibling,
          buttonId: request.buttonId,
          localizationKey: request.localizationKey,
          enabled: request.enabled,
          owner: request.owner,
          onCommand: () => {},
          readLabel: () => ++reads === 1 ? "Initial label" : null,
          observeTranslations: (update) => translationChanged = update,
          reportFailure: (owner, error) => {
            reportAccepted = connection?.reportFailure(owner, error) ?? false;
          },
        });
        viewRef.current = mounted;
        translationChanged();
        return trigger.getAttribute("open") === "true"
          ? "rendered"
          : "deferred";
      },
      setEnabled: () => {},
      remove: () => {
        viewRef.current?.dispose();
        viewRef.current = null;
      },
    });
    const lease = registry.beginGeneration(
      {
        featureId: HUB_PANEL_MENU_FEATURE_ID,
        hostEpoch: "owned-view-race",
        generation: 1,
      },
      () => hostFailures++,
      () => false,
    );
    registry.stageDescriptor(lease, { command: "open-hub" });
    let readinessError = "";
    try {
      await registry.stageAndWait(
        lease,
        [browserWindow],
        new AbortController().signal,
        500,
      );
    } catch (error) {
      const message = typeof error === "object" && error !== null
        ? Reflect.get(error, "message")
        : undefined;
      readinessError = typeof message === "string" ? message : String(error);
    }
    assertEquals(
      readinessError,
      "invalid HUB menu label",
      "initial translation failure rejects readiness synchronously",
    );
    assert(
      reportAccepted,
      "unacknowledged current delivery can report failure",
    );
    assertEquals(
      hostFailures,
      1,
      "translation failure is sticky and reported once",
    );
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      0,
      "failed initial render does not leave a readiness waiter",
    );
    assertEquals(
      viewRef.current?.node.getAttribute("label"),
      "Initial label",
      "failed observed update does not replace its valid initial label",
    );
    registry.revoke(lease);
  } finally {
    if (connection) connection.dispose();
    claim?.release();
    viewRef.current?.dispose();
    fixture.container.remove();
  }
}

await runTests("HubPanelMenu.test.ts", [
  {
    name: "owned view captures its button and preserves Settings order",
    fn: testOwnedViewUsesItsExactElementAndSettingsSlot,
  },
  {
    name: "focus claim restores only without user movement",
    fn: testFocusClaimRestoresOnlyWithoutUserMovement,
  },
  {
    name: "delayed old cleanup preserves replacement node and handler",
    fn: testDelayedOldCleanupPreservesReplacementNodeAndHandler,
  },
  {
    name: "invalid initial labels reject and clean up setup",
    fn: testInvalidInitialLabelsRejectAndCleanSetup,
  },
  {
    name: "late translation failures are reported by the owned view",
    fn: testLateTranslationFailureReportsThroughOwnedView,
  },
  {
    name: "translation cleanup errors still remove owned resources",
    fn: testTranslationCleanupFailureStillRemovesOwnedResources,
  },
  {
    name: "focus transfer errors still remove owned resources",
    fn: testFocusTransferFailureStillRemovesOwnedResources,
  },
  {
    name: "readiness observer waits and disconnects at readiness or disposal",
    fn: testReadinessObserverLifetimeIsBounded,
  },
  {
    name: "initial observed translation failure rejects adapter readiness",
    fn: testInitialMountTranslationFailureFencesReadiness,
  },
]);
