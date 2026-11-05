// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertStringArrayEqual,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import { WEB_PANEL_CLIPBOARD_SANDBOX_SOURCE } from "../NRWebPanelClipboardChild.sys.mts";
import { isFocusedNestedBrowser } from "../NRWebPanelClipboardParent.sys.mts";
import {
  isWebPanelBrowserId,
  type NRWebPanelClipboardReadResult,
  WEB_PANEL_BROWSER_IDS_SHARED_KEY,
  WEB_PANEL_CLIPBOARD_FLAVORS,
} from "../../common/NRWebPanelClipboardTypes.ts";
import { WEB_PANEL_BROWSER_IDS_SHARED_KEY as REGISTRY_SHARED_KEY } from "../../../chrome/common/panel-sidebar/utils/web-panel-browser-registry.ts";

const NOT_ALLOWED = "Clipboard read operation is not allowed.";
const NO_ACTIVATION =
  "Clipboard read request was blocked due to lack of user activation.";

type ReadKind = "read" | "readText";
type FakeClipboard = {
  read(): Promise<unknown>;
  readText(): Promise<unknown>;
};

class FakeClipboardItem {
  constructor(readonly record: Record<string, Blob>) {}
}

interface Harness {
  clipboard: FakeClipboard;
  requests: ReadKind[];
  sandbox: object;
}

/**
 * Evaluates the wrapper against a fake page window whose original methods
 * return `original(kind)` and whose parent answers with `response`.
 */
function installWrapper(
  original: (kind: ReadKind) => Promise<unknown>,
  response: NRWebPanelClipboardReadResult,
): Harness {
  function FakeClipboardCtor() {}
  FakeClipboardCtor.prototype.read = function read() {
    return original("read");
  };
  FakeClipboardCtor.prototype.readText = function readText() {
    return original("readText");
  };

  const fakeWindow: Record<string, unknown> = {
    Clipboard: FakeClipboardCtor,
    Promise,
    Array,
    Object,
    Blob,
    ClipboardItem: FakeClipboardItem,
    DOMException,
  };
  fakeWindow.window = fakeWindow;

  const sandbox = Cu.Sandbox(
    Services.scriptSecurityManager.getSystemPrincipal(),
    { sandboxPrototype: fakeWindow, wantXrays: false },
  );
  const requests: ReadKind[] = [];
  const install = Cu.evalInSandbox(
    WEB_PANEL_CLIPBOARD_SANDBOX_SOURCE,
    sandbox,
  ) as (request: unknown) => void;
  install(
    Cu.exportFunction(
      (kind: ReadKind, callback: (value: unknown) => void) => {
        requests.push(kind);
        callback(Cu.cloneInto(response, sandbox));
      },
      sandbox,
    ),
  );

  return {
    clipboard: new (FakeClipboardCtor as unknown as new () => FakeClipboard)(),
    requests,
    sandbox,
  };
}

function rejectWith(message: string): () => Promise<unknown> {
  return () => Promise.reject(new DOMException(message, "NotAllowedError"));
}

async function rejectionOf(promise: Promise<unknown>): Promise<DOMException> {
  try {
    await promise;
  } catch (error) {
    return error as DOMException;
  }
  throw new Error("expected the promise to reject");
}

async function testFallsBackForReadRejection(): Promise<void> {
  const png = new Uint8Array([137, 80, 78, 71]).buffer;
  const { clipboard, requests } = installWrapper(rejectWith(NOT_ALLOWED), {
    status: "ok",
    entries: [
      { type: "text/plain", text: "hello" },
      { type: "image/png", bytes: png },
    ],
  });

  const items = await clipboard.read() as FakeClipboardItem[];
  assertStringArrayEqual(requests, ["read"], "parent is asked once");
  assertEquals(items.length, 1, "one clipboard item, like Gecko");
  const record = items[0].record;
  assertStringArrayEqual(
    Object.keys(record),
    ["text/plain", "image/png"],
    "entries keep the parent's order",
  );
  assertEquals(await record["text/plain"].text(), "hello", "text blob");
  assertEquals(record["image/png"].type, "image/png", "image blob type");
  assertEquals(record["image/png"].size, 4, "image blob carries the bytes");
}

async function testFallsBackForReadText(): Promise<void> {
  const { clipboard, requests } = installWrapper(rejectWith(NOT_ALLOWED), {
    status: "ok",
    entries: [{ type: "text/plain", text: "hello" }],
  });
  assertEquals(await clipboard.readText(), "hello", "readText resolves text");
  assertStringArrayEqual(requests, ["readText"], "readText kind is sent");
}

async function testEmptyClipboard(): Promise<void> {
  const { clipboard } = installWrapper(rejectWith(NOT_ALLOWED), {
    status: "ok",
    entries: [],
  });
  const items = await clipboard.read() as unknown[];
  assertEquals(items.length, 0, "read() resolves to an empty array");
  assertEquals(await clipboard.readText(), "", "readText() resolves to ''");
}

async function testDeniedAndNoActivationReject(): Promise<void> {
  const denied = installWrapper(rejectWith(NOT_ALLOWED), {
    status: "denied",
  });
  const deniedError = await rejectionOf(denied.clipboard.read());
  assertEquals(deniedError.name, "NotAllowedError", "denied error name");
  assertEquals(deniedError.message, NOT_ALLOWED, "denied error message");

  const inactive = installWrapper(rejectWith(NOT_ALLOWED), {
    status: "no-activation",
  });
  const inactiveError = await rejectionOf(inactive.clipboard.readText());
  assertEquals(inactiveError.message, NO_ACTIVATION, "activation message");
}

async function testOtherOutcomesPassThrough(): Promise<void> {
  const activation = installWrapper(rejectWith(NO_ACTIVATION), {
    status: "ok",
    entries: [],
  });
  const error = await rejectionOf(activation.clipboard.read());
  assertEquals(error.message, NO_ACTIVATION, "Gecko's error is kept");
  assertEquals(activation.requests.length, 0, "no fallback for activation");

  const success = installWrapper(() => Promise.resolve("native"), {
    status: "ok",
    entries: [],
  });
  assertEquals(await success.clipboard.readText(), "native", "native result");
  assertEquals(success.requests.length, 0, "no fallback after success");
}

function testRequestIsNotOnTheSandboxGlobal(): void {
  const { sandbox } = installWrapper(rejectWith(NOT_ALLOWED), {
    status: "denied",
  });
  assertEquals(
    Cu.evalInSandbox(
      "Object.getOwnPropertyNames(this).filter(n => /paste|request/i.test(n)).length",
      sandbox,
    ),
    0,
    "the privileged request function is only reachable from the wrappers",
  );
}

function testWrapperLooksLikeTheMethod(): void {
  const { clipboard } = installWrapper(rejectWith(NOT_ALLOWED), {
    status: "denied",
  });
  assertEquals(clipboard.read.name, "read", "read keeps its name");
  assertEquals(clipboard.readText.name, "readText", "readText keeps its name");
  assertEquals(clipboard.read.length, 0, "read keeps its length");
}

type FakeElement = { ownerDocument: FakeDoc };
type FakeDoc = {
  activeElement: unknown;
  hasFocus(): boolean;
  defaultView: {
    browsingContext: { embedderElement: FakeElement | null } | null;
  };
};

function nestedPanel(): {
  outer: FakeElement;
  inner: FakeElement;
  topDoc: FakeDoc & { focused: boolean };
  panelDoc: FakeDoc;
} {
  const topDoc = {
    activeElement: null as unknown,
    focused: true,
    hasFocus() {
      return this.focused;
    },
    defaultView: { browsingContext: { embedderElement: null } },
  };
  const outer: FakeElement = { ownerDocument: topDoc };
  const panelDoc: FakeDoc = {
    activeElement: null as unknown,
    hasFocus: () => true,
    defaultView: { browsingContext: { embedderElement: outer } },
  };
  const inner: FakeElement = { ownerDocument: panelDoc };
  topDoc.activeElement = outer;
  panelDoc.activeElement = inner;
  return { outer, inner, topDoc, panelDoc };
}

function testFocusChain(): void {
  const panel = nestedPanel();
  assert(isFocusedNestedBrowser(panel.inner), "focused web panel browser");

  panel.topDoc.focused = false;
  assert(!isFocusedNestedBrowser(panel.inner), "window without focus");
  panel.topDoc.focused = true;

  panel.topDoc.activeElement = {};
  assert(!isFocusedNestedBrowser(panel.inner), "another panel/tab focused");
  panel.topDoc.activeElement = panel.outer;

  panel.panelDoc.activeElement = {};
  assert(!isFocusedNestedBrowser(panel.inner), "inner browser not focused");

  // A browser directly in the top window is a normal tab, handled by Gecko.
  assert(!isFocusedNestedBrowser(panel.outer), "tab browsers are rejected");
  assert(!isFocusedNestedBrowser(null), "missing browser");
}

function testSharedKeyMatchesRegistry(): void {
  // The panel sidebar publishes web panel browserIds under its own copy of
  // this key; the actor reads them with this one.
  assertEquals(
    WEB_PANEL_BROWSER_IDS_SHARED_KEY,
    REGISTRY_SHARED_KEY,
    "actor and registry must use the same shared data key",
  );
}

function testSharedHelpers(): void {
  assert(isWebPanelBrowserId([3, 4], 4), "listed id");
  assert(!isWebPanelBrowserId([3, 4], 5), "unlisted id");
  assert(!isWebPanelBrowserId([0], 0), "id 0 (uninitialized) never matches");
  assert(!isWebPanelBrowserId(undefined, 4), "missing list");
  assert(!isWebPanelBrowserId("4", 4), "malformed list");
  assertStringArrayEqual(
    [...WEB_PANEL_CLIPBOARD_FLAVORS.read],
    ["text/html", "text/plain", "image/png"],
    "read() uses Gecko's mandatory types in order",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "falls back to the parent for Gecko's focused-tab rejection",
      fn: testFallsBackForReadRejection,
    },
    { name: "readText fallback resolves text", fn: testFallsBackForReadText },
    { name: "empty clipboard matches Gecko", fn: testEmptyClipboard },
    {
      name: "denied and no-activation reject with NotAllowedError",
      fn: testDeniedAndNoActivationReject,
    },
    {
      name: "other outcomes pass through untouched",
      fn: testOtherOutcomesPassThrough,
    },
    {
      name: "request function is not exposed on the sandbox global",
      fn: testRequestIsNotOnTheSandboxGlobal,
    },
    {
      name: "wrappers keep the method name and length",
      fn: testWrapperLooksLikeTheMethod,
    },
    { name: "focus chain mirrors Gecko's check", fn: testFocusChain },
    { name: "shared helpers", fn: testSharedHelpers },
    {
      name: "shared data key matches the panel sidebar registry",
      fn: testSharedKeyMatchesRegistry,
    },
  ];
  await runTests("NRWebPanelClipboardActors.test.ts", tests);
}
