// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type {
  ChromeWebStoreInstallInfo,
  ChromeWebStoreWindow,
  InstallInfo,
  XPInstallObserver,
} from "../types.ts";
import { createAddonInstance, createAddonWindow } from "./fixtures.ts";

const CWS_TOPIC = "floorp-chrome-web-store-install-started";
const PERMISSION_TOPIC = "webextension-permission-prompt";

function observerCount(topic: string): number {
  const observers = Services.obs.enumerateObservers(topic);
  let count = 0;
  while (observers.hasMoreElements()) {
    observers.getNext();
    count++;
  }
  return count;
}

function installNativeConfirmation(win: ChromeWebStoreWindow) {
  const calls: { receiver: XPInstallObserver; browser: unknown }[] = [];
  const original = function (
    this: XPInstallObserver,
    browser: unknown,
    _info: InstallInfo,
  ): void {
    calls.push({ receiver: this, browser });
  };
  win.gXPInstallObserver = { showInstallConfirmation: original };
  return { original, calls };
}

function createPrompt(win: ChromeWebStoreWindow, text: string): HTMLElement {
  const doc = win.document;
  const notification = doc.createElement("div");
  notification.id = "addon-webext-permissions-notification";
  const content = doc.createElement("popupnotificationcontent");
  const description = doc.createElement("div");
  description.className = "popup-notification-description";
  description.textContent = text;
  const intro = doc.createElement("div");
  intro.id = "addon-webext-perm-intro";
  notification.append(content, description, intro);
  doc.body.append(notification);
  return notification;
}

function promptSubject(win: ChromeWebStoreWindow) {
  return { wrappedJSObject: { target: { documentGlobal: win } } };
}

function startInstall(
  win: ChromeWebStoreWindow,
  info: ChromeWebStoreInstallInfo,
) {
  // Match the real actor contract: a window-local marker precedes the shared
  // notification, whose payload deliberately contains no window reference.
  win.__chromeWebStoreInstallInfo = info;
  Services.obs.notifyObservers({ wrappedJSObject: info }, CWS_TOPIC);
}

function testConstructorOwnershipAndReopen(): void {
  const before = [observerCount(CWS_TOPIC), observerCount(PERMISSION_TOPIC)];
  const shared = Cu.getGlobalForObject(Services) as Record<string, unknown>;
  const sharedContext = shared.__floorpAddonsContext;
  const first = createAddonWindow("first addon owner");
  const second = createAddonWindow("second addon owner");
  const reopened = createAddonWindow("reopened addon owner");
  const firstNative = installNativeConfirmation(first.win);
  installNativeConfirmation(second.win);
  installNativeConfirmation(reopened.win);
  try {
    const firstInstance = createAddonInstance(first.win);
    createAddonInstance(second.win);
    const context = first.win.__floorpAddonsContext;
    assert(
      context?.observer && context.customizer,
      "constructor retains handles created inside super",
    );
    assert(
      first.win.gXPInstallObserver?.showInstallConfirmation !==
        firstNative.original,
      "constructor retains its native override",
    );
    const oldObserver = context.observer;
    firstInstance.init();
    assertEquals(
      first.win.__floorpAddonsContext,
      context,
      "repeated init preserves the active owner",
    );
    for (const [index, topic] of [CWS_TOPIC, PERMISSION_TOPIC].entries()) {
      assertEquals(
        observerCount(topic),
        before[index] + 2,
        "one observer per live window and topic",
      );
    }
    context.savedDescriptionChildren = [
      first.win.document.createTextNode("saved"),
    ];
    first.close();
    assertEquals(
      first.win.__floorpAddonsContext,
      undefined,
      "closed owner drops its context",
    );
    assertEquals(context.observer, null, "closed owner drops observer handle");
    assertEquals(context.customizer, null, "closed owner drops customizer");
    assertEquals(context.logger, null, "closed owner drops logger");
    assertEquals(
      context.savedDescriptionChildren,
      null,
      "closed owner drops document clones",
    );
    assertEquals(
      first.win.gXPInstallObserver?.showInstallConfirmation,
      firstNative.original,
      "close restores the exact native function",
    );
    firstInstance.init();
    assertEquals(
      first.win.__floorpAddonsContext,
      undefined,
      "an old instance cannot revive after close",
    );
    for (const [index, topic] of [CWS_TOPIC, PERMISSION_TOPIC].entries()) {
      assertEquals(
        observerCount(topic),
        before[index] + 1,
        "closing one owner preserves the other subscription",
      );
    }
    startInstall(second.win, { name: "Surviving owner", id: "survivor" });
    assertEquals(
      second.win.__floorpAddonsContext?.state.pendingChromeWebStoreInstall?.id,
      "survivor",
      "other window still receives CWS installs after first owner closes",
    );
    createAddonInstance(reopened.win);
    assert(
      reopened.win.__floorpAddonsContext?.observer !== oldObserver,
      "reopening registers a fresh observer",
    );
    assertEquals(
      shared.__floorpAddonsContext,
      sharedContext,
      "no window-owned context is stored on the shared Services global",
    );
  } finally {
    first.close();
    second.close();
    reopened.close();
  }
  for (const [index, topic] of [CWS_TOPIC, PERMISSION_TOPIC].entries()) {
    assertEquals(
      observerCount(topic),
      before[index],
      "all test subscriptions are released",
    );
  }
}

function testPromptRoutingAndNativeContent(): void {
  const first = createAddonWindow("first prompt owner");
  const second = createAddonWindow("second prompt owner");
  const native = installNativeConfirmation(first.win);
  const firstPrompt = createPrompt(first.win, "Firefox permission A");
  const secondPrompt = createPrompt(second.win, "Firefox permission B");
  try {
    createAddonInstance(first.win);
    createAddonInstance(second.win);
    const firstContext = first.win.__floorpAddonsContext;
    const secondContext = second.win.__floorpAddonsContext;
    assert(
      firstContext?.observer && secondContext?.observer,
      "both owners have observers",
    );
    const a = { name: "Chrome Extension A", id: "extension-a" };
    const b = { name: "Chrome Extension B", id: "extension-b" };
    startInstall(first.win, a);
    assertEquals(
      secondContext.state.pendingChromeWebStoreInstall,
      null,
      "shared CWS notification does not update another owner",
    );
    startInstall(second.win, b);
    const subject = promptSubject(second.win);
    firstContext.observer.observe(subject, PERMISSION_TOPIC, null);
    secondContext.observer.observe(subject, PERMISSION_TOPIC, null);
    assertEquals(
      first.frames.size,
      0,
      "foreign permission prompt schedules no owner work",
    );
    second.flushFrames();
    second.flushFrames();
    assert(
      secondPrompt.querySelector(".chrome-web-store-message")?.textContent
        ?.includes(b.name),
      "target prompt contains its own extension name",
    );
    assertEquals(
      firstPrompt.querySelector(".popup-notification-description")?.textContent,
      "Firefox permission A",
      "other owner's DOM is untouched",
    );
    assertEquals(
      firstContext.state.pendingChromeWebStoreInstall?.id,
      a.id,
      "other owner's pending install is preserved",
    );
    assertEquals(
      secondContext.state.pendingChromeWebStoreInstall,
      null,
      "only completed owner's state is cleared",
    );

    // Native ExtensionsUI replaces text before notifying a normal Firefox
    // prompt. Cleanup must not restore the previous CWS description over it.
    const description = secondPrompt.querySelector(
      ".popup-notification-description",
    );
    assert(description, "target has a description");
    description.textContent = "Fresh native Firefox permission text";
    secondContext.observer.observe(subject, PERMISSION_TOPIC, null);
    second.flushFrames();
    second.flushFrames();
    assertEquals(
      description.textContent,
      "Fresh native Firefox permission text",
      "normal Firefox prompt keeps its newly rendered text",
    );
    assertEquals(
      secondPrompt.querySelector(".chrome-extension-warning"),
      null,
      "normal prompt removes the previous CWS warning",
    );
    assertEquals(
      secondPrompt.hasAttribute("data-cws-customized"),
      false,
      "normal prompt clears the CWS marker",
    );

    // Native getTabBrowser walks from an extension document's handler to the
    // browser element in a chrome document, including sidebar top windows.
    const nestedSubject = {
      wrappedJSObject: {
        target: {
          documentGlobal: {
            docShell: {
              itemType: Ci.nsIDocShell.typeContent,
              chromeEventHandler: { documentGlobal: first.win },
            },
          },
        },
      },
    };
    firstContext.observer.observe(nestedSubject, PERMISSION_TOPIC, null);
    first.flushFrames();
    first.flushFrames();
    assert(
      firstPrompt.querySelector(".chrome-web-store-message")?.textContent
        ?.includes(a.name),
      "nested native target resolves to the correct owner",
    );
    const sidebarSubject = {
      wrappedJSObject: {
        target: {
          documentGlobal: {
            docShell: { itemType: Ci.nsIDocShell.typeChrome },
            browsingContext: { topChromeWindow: second.win },
          },
          getAttribute: () => "sidebar",
        },
      },
    };
    firstContext.observer.observe(sidebarSubject, PERMISSION_TOPIC, null);
    secondContext.observer.observe(sidebarSubject, PERMISSION_TOPIC, null);
    assertEquals(
      first.frames.size,
      0,
      "sidebar routed to another owner does not schedule work here",
    );
    assertEquals(
      second.frames.size,
      1,
      "sidebar resolves its top chrome window",
    );
    second.flushFrames();
    second.flushFrames();

    startInstall(first.win, a);
    first.win.gXPInstallObserver?.showInstallConfirmation({
      documentGlobal: second.win,
    }, { installs: [] });
    assertEquals(
      native.calls.length,
      1,
      "foreign target still reaches the original native confirmation",
    );
    assertEquals(
      native.calls[0].receiver,
      first.win.gXPInstallObserver,
      "native receiver is preserved",
    );
    assertEquals(
      first.frames.size,
      0,
      "native override does not customize another window's target",
    );
    first.close();
    startInstall(second.win, b);
    secondContext.observer.observe(subject, PERMISSION_TOPIC, null);
    second.flushFrames();
    second.flushFrames();
    assert(
      secondPrompt.querySelector(".chrome-web-store-message")?.textContent
        ?.includes(b.name),
      "remaining owner still customizes prompts after first owner closes",
    );
  } finally {
    first.close();
    second.close();
  }
}

function testCloseAndReplacementCancelDeferredWork(): void {
  const fixture = createAddonWindow("deferred addon owner");
  installNativeConfirmation(fixture.win);
  const prompt = createPrompt(fixture.win, "Unmodified native text");
  try {
    const instance = createAddonInstance(fixture.win);
    const context = fixture.win.__floorpAddonsContext;
    assert(context?.observer, "owner starts with an observer");
    startInstall(fixture.win, { name: "Cancelled extension", id: "cancelled" });
    context.observer.observe(
      promptSubject(fixture.win),
      PERMISSION_TOPIC,
      null,
    );
    const staleCallbacks = Array.from(fixture.frames.values());
    assertEquals(staleCallbacks.length, 1, "customization is deferred");
    // This is the same cleanup registered with the Nora/Solid HMR root.
    (instance as unknown as { cleanup: () => void }).cleanup();
    assertEquals(
      fixture.frames.size,
      0,
      "HMR cleanup cancels scheduled frames",
    );
    instance.init();
    assertEquals(
      fixture.win.__floorpAddonsContext,
      undefined,
      "disposed instance cannot re-register",
    );
    createAddonInstance(fixture.win);
    const replacement = fixture.win.__floorpAddonsContext;
    assert(
      replacement?.observer && replacement !== context,
      "replacement owns a fresh context",
    );
    for (const callback of staleCallbacks) callback(performance.now());
    assertEquals(
      prompt.querySelector(".chrome-web-store-message"),
      null,
      "stale callback cannot mutate a replacement owner's UI",
    );
    assertEquals(
      replacement.state.pendingChromeWebStoreInstall,
      null,
      "stale callback cannot seed replacement state",
    );
    startInstall(fixture.win, { name: "Close cancellation", id: "closed" });
    replacement.observer.observe(
      promptSubject(fixture.win),
      PERMISSION_TOPIC,
      null,
    );
    const closedCallbacks = Array.from(fixture.frames.values());
    fixture.close();
    assertEquals(
      fixture.frames.size,
      0,
      "window close cancels deferred frames",
    );
    for (const callback of closedCallbacks) callback(performance.now());
    assertEquals(
      prompt.querySelector(".chrome-web-store-message"),
      null,
      "closed owner ignores already-delivered frame callbacks",
    );
  } finally {
    fixture.close();
  }
}

function testCloseBeforeLoadDoesNotRevive(): void {
  const fixture = createAddonWindow("loading addon owner");
  const native = installNativeConfirmation(fixture.win);
  Object.defineProperty(fixture.win.document, "readyState", {
    value: "loading",
  });
  const before = observerCount(CWS_TOPIC);
  try {
    const instance = createAddonInstance(fixture.win);
    assert(
      fixture.win.__floorpAddonsContext?.observer,
      "observer exists during initial document load",
    );
    assertEquals(
      fixture.win.gXPInstallObserver?.showInstallConfirmation,
      native.original,
      "native override waits for load",
    );
    fixture.close();
    fixture.win.dispatchEvent(new Event("load"));
    instance.init();
    assertEquals(
      fixture.win.gXPInstallObserver?.showInstallConfirmation,
      native.original,
      "late load cannot install an override after close",
    );
    assertEquals(
      fixture.win.__floorpAddonsContext,
      undefined,
      "late load leaves no owner context",
    );
    assertEquals(
      observerCount(CWS_TOPIC),
      before,
      "late load does not restore an observer subscription",
    );
  } finally {
    fixture.close();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("addons/lifecycle.test.ts", [
    {
      name: "real constructors own observers through close and reopen",
      fn: testConstructorOwnershipAndReopen,
    },
    {
      name:
        "multiple windows route CWS and native permission prompts independently",
      fn: testPromptRoutingAndNativeContent,
    },
    {
      name: "close and HMR replacement cancel deferred owner work",
      fn: testCloseAndReplacementCancelDeferredWork,
    },
    {
      name: "load after close cannot revive an owner",
      fn: testCloseBeforeLoadDoesNotRevive,
    },
  ]);
}
