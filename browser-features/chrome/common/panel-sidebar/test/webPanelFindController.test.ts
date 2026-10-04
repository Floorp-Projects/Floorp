// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  WebPanelFindController,
  type WebPanelFindDocument,
  type WebPanelFindWindow,
} from "../utils/web-panel-find-controller.ts";
import type { WebPanelBrowserElement } from "../utils/web-panel-browser.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const FIND_COMMAND_IDS = [
  "cmd_find",
  "cmd_findAgain",
  "cmd_findPrevious",
  "cmd_findSelection",
] as const;

class FakeCommandElement extends EventTarget {
  constructor(readonly id: string) {
    super();
  }
}

class FakeFindbarElement extends EventTarget {
  id = "";
  browser: WebPanelBrowserElement | null = null;
  isConnected = false;
  destroyCount = 0;
  removeCount = 0;
  closeCount = 0;
  readonly closeButton = new EventTarget();
  readonly calls: string[] = [];

  constructor() {
    super();
    this.addEventListener(
      "keypress",
      (event) => {
        if ((event as KeyboardEvent).keyCode === 27) {
          this.close();
          event.preventDefault();
        }
      },
      true,
    );
    this.closeButton.addEventListener("command", () => this.close());
  }

  private close(): void {
    this.closeCount += 1;
  }

  onFindCommand(): void {
    this.calls.push("find");
  }

  onFindAgainCommand(findPrevious: boolean): void {
    this.calls.push(`again:${String(findPrevious)}`);
  }

  onFindSelectionCommand(): void {
    this.calls.push("selection");
  }

  destroy(): void {
    this.destroyCount += 1;
  }

  remove(): void {
    this.removeCount += 1;
    this.isConnected = false;
  }
}

class FakeBrowserElement {
  isConnected = true;
  insertedFindbar: FakeFindbarElement | null = null;

  insertAdjacentElement(_position: string, element: Element): Element | null {
    const findbar = element as unknown as FakeFindbarElement;
    findbar.isConnected = true;
    this.insertedFindbar = findbar;
    return element;
  }
}

class FakeDocument implements WebPanelFindDocument {
  private readonly commands = new Map<string, FakeCommandElement>();
  readonly createdFindbars: FakeFindbarElement[] = [];

  constructor(commandIds: readonly string[]) {
    for (const commandId of commandIds) {
      this.commands.set(commandId, new FakeCommandElement(commandId));
    }
  }

  getElementById(id: string): Element | null {
    const command = this.commands.get(id);
    if (command) {
      return command as unknown as Element;
    }

    const findbar = this.createdFindbars.find((element) => element.id === id);
    return (findbar as unknown as Element | undefined) ?? null;
  }

  createXULElement(name: string): Element {
    assertEquals(name, "findbar", "controller should only create a findbar");
    const findbar = new FakeFindbarElement();
    this.createdFindbars.push(findbar);
    return findbar as unknown as Element;
  }

  getCommand(id: string): FakeCommandElement {
    const command = this.commands.get(id);
    assert(command, `missing fake command: ${id}`);
    return command;
  }
}

class FakeWindow extends EventTarget implements WebPanelFindWindow {
  private readonly animationFrameCallbacks: FrameRequestCallback[] = [];

  requestAnimationFrame(callback: FrameRequestCallback): number {
    this.animationFrameCallbacks.push(callback);
    return this.animationFrameCallbacks.length;
  }

  flushAnimationFrames(): void {
    const callbacks = this.animationFrameCallbacks.splice(0);
    for (const callback of callbacks) {
      callback(0);
    }
  }
}

type ControllerHarness = {
  browser: FakeBrowserElement;
  controller: WebPanelFindController;
  document: FakeDocument;
  window: FakeWindow;
};

function createHarness(
  commandIds: readonly string[] = FIND_COMMAND_IDS,
): ControllerHarness {
  const browser = new FakeBrowserElement();
  const panelDocument = new FakeDocument(commandIds);
  const panelWindow = new FakeWindow();
  const controller = new WebPanelFindController(
    browser as unknown as WebPanelBrowserElement,
    panelDocument,
    panelWindow,
  );
  return { browser, controller, document: panelDocument, window: panelWindow };
}

function dispatchCommand(command: FakeCommandElement): Event {
  const event = new Event("command", { bubbles: true, cancelable: true });
  command.dispatchEvent(event);
  return event;
}

async function settleFindbarCreation(panelWindow: FakeWindow): Promise<void> {
  panelWindow.flushAnimationFrames();
  await new Promise<void>((resolve) => globalThis.setTimeout(resolve, 0));
}

async function testRoutesOnlyFindCommandsToOneLazyFindbar(): Promise<void> {
  const harness = createHarness();
  let builtInHandlerCalls = 0;
  for (const commandId of FIND_COMMAND_IDS) {
    harness.document.getCommand(commandId).addEventListener("command", () => {
      builtInHandlerCalls += 1;
    });
  }

  harness.controller.init();
  harness.controller.init();

  assertEquals(
    harness.document.createdFindbars.length,
    0,
    "init should not eagerly create a findbar",
  );

  const findEvent = dispatchCommand(harness.document.getCommand("cmd_find"));
  assert(findEvent.defaultPrevented, "cmd_find should be intercepted");
  await settleFindbarCreation(harness.window);

  assertEquals(
    harness.document.createdFindbars.length,
    1,
    "the first find command should create exactly one findbar",
  );
  const findbar = harness.document.createdFindbars[0];
  assertEquals(
    findbar.browser,
    harness.browser as unknown as WebPanelBrowserElement,
    "the findbar should bind directly to the web panel content browser",
  );

  dispatchCommand(harness.document.getCommand("cmd_findAgain"));
  dispatchCommand(harness.document.getCommand("cmd_findPrevious"));
  dispatchCommand(harness.document.getCommand("cmd_findSelection"));
  await settleFindbarCreation(harness.window);

  assertEquals(
    findbar.calls.join(","),
    "find,again:false,again:true,selection",
    "each supported command should route to the matching findbar API",
  );
  assertEquals(
    builtInHandlerCalls,
    0,
    "intercepted commands should not reach the browser.xhtml handler",
  );
  assertEquals(
    harness.document.createdFindbars.length,
    1,
    "repeated commands and double init should reuse the findbar",
  );
}

function testLeavesCloseAndUnrelatedCommandsUntouched(): void {
  const harness = createHarness([
    "cmd_find",
    "cmd_findAgain",
    "cmd_findPrevious",
    "cmd_findClose",
    "Browser:Reload",
  ]);
  harness.controller.init();

  let untouchedHandlerCalls = 0;
  const closeCommand = harness.document.getCommand("cmd_findClose");
  const reloadCommand = harness.document.getCommand("Browser:Reload");
  closeCommand.addEventListener("command", () => untouchedHandlerCalls += 1);
  reloadCommand.addEventListener("command", () => untouchedHandlerCalls += 1);

  const closeEvent = dispatchCommand(closeCommand);
  const reloadEvent = dispatchCommand(reloadCommand);

  assert(!closeEvent.defaultPrevented, "cmd_findClose should remain untouched");
  assert(
    !reloadEvent.defaultPrevented,
    "unrelated commands should remain untouched",
  );
  assertEquals(
    untouchedHandlerCalls,
    2,
    "untargeted command handlers should still run",
  );
  assertEquals(
    harness.document.createdFindbars.length,
    0,
    "untargeted commands should not create a findbar",
  );
}

async function testLeavesFindbarEscapeAndCloseBehaviorUntouched(): Promise<
  void
> {
  const harness = createHarness();
  harness.controller.init();
  dispatchCommand(harness.document.getCommand("cmd_find"));
  await settleFindbarCreation(harness.window);

  const findbar = harness.document.createdFindbars[0];
  const escapeEvent = new KeyboardEvent("keypress", {
    key: "Escape",
    keyCode: 27,
    bubbles: true,
    cancelable: true,
  });
  findbar.dispatchEvent(escapeEvent);
  findbar.closeButton.dispatchEvent(
    new Event("command", { bubbles: true, cancelable: true }),
  );

  assert(escapeEvent.defaultPrevented, "the findbar should handle Escape");
  assertEquals(
    findbar.closeCount,
    2,
    "the findbar should retain ownership of Escape and its close button",
  );
}

async function testUnloadDestroysAndDetachesOnce(): Promise<void> {
  const harness = createHarness();
  harness.controller.init();
  dispatchCommand(harness.document.getCommand("cmd_find"));
  await settleFindbarCreation(harness.window);

  const findbar = harness.document.createdFindbars[0];
  harness.window.dispatchEvent(new Event("unload"));
  harness.controller.destroy();

  assertEquals(findbar.destroyCount, 1, "findbar should be destroyed once");
  assertEquals(findbar.removeCount, 1, "findbar should be removed once");

  let commandCallsAfterDestroy = 0;
  const command = harness.document.getCommand("cmd_find");
  command.addEventListener("command", () => commandCallsAfterDestroy += 1);
  const event = dispatchCommand(command);

  assert(!event.defaultPrevented, "destroy should detach the command listener");
  assertEquals(
    commandCallsAfterDestroy,
    1,
    "commands should continue normally after controller destruction",
  );
  assertEquals(
    findbar.calls.length,
    1,
    "destroyed controller should not dispatch another find command",
  );
}

async function testDestroyDuringLazyCreationIsSafe(): Promise<void> {
  const harness = createHarness();
  harness.controller.init();
  dispatchCommand(harness.document.getCommand("cmd_find"));

  assertEquals(
    harness.document.createdFindbars.length,
    1,
    "the command should begin lazy findbar creation",
  );
  const findbar = harness.document.createdFindbars[0];
  harness.controller.destroy();
  await settleFindbarCreation(harness.window);

  assertEquals(
    findbar.browser,
    null,
    "destroyed pending findbar should not bind",
  );
  assertEquals(
    findbar.calls.length,
    0,
    "destroyed pending command should not run",
  );
  assertEquals(
    findbar.destroyCount,
    1,
    "pending findbar cleanup should be idempotent",
  );
  assertEquals(
    findbar.removeCount,
    1,
    "pending findbar should be removed once",
  );
}

async function testPanelReopenUsesIndependentController(): Promise<void> {
  const firstPanel = createHarness();
  firstPanel.controller.init();
  dispatchCommand(firstPanel.document.getCommand("cmd_find"));
  await settleFindbarCreation(firstPanel.window);
  firstPanel.window.dispatchEvent(new Event("unload"));

  const reopenedPanel = createHarness();
  reopenedPanel.controller.init();
  dispatchCommand(reopenedPanel.document.getCommand("cmd_find"));
  await settleFindbarCreation(reopenedPanel.window);

  assertEquals(
    reopenedPanel.document.createdFindbars.length,
    1,
    "a reopened panel should create its own findbar",
  );
  assertEquals(
    reopenedPanel.document.createdFindbars[0].calls.join(","),
    "find",
    "find should work after reopening the panel",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "routes only find commands to one lazy findbar",
      fn: testRoutesOnlyFindCommandsToOneLazyFindbar,
    },
    {
      name: "leaves close and unrelated commands untouched",
      fn: testLeavesCloseAndUnrelatedCommandsUntouched,
    },
    {
      name: "leaves findbar Escape and close behavior untouched",
      fn: testLeavesFindbarEscapeAndCloseBehaviorUntouched,
    },
    {
      name: "unload destroys and detaches once",
      fn: testUnloadDestroysAndDetachesOnce,
    },
    {
      name: "destroy during lazy creation is safe",
      fn: testDestroyDuringLazyCreationIsSafe,
    },
    {
      name: "panel reopen uses an independent controller",
      fn: testPanelReopenUsesIndependentController,
    },
  ];

  await runTests("webPanelFindController.test.ts", tests);
}
