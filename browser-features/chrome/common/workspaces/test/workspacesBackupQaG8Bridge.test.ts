// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA group g8 (PR #3159): the NRWorkspaces content bridge used by the Hub
// backup page. The existing tests call window.workspacesBackupFuncs directly
// and never exercise the actor child that the page actually talks to.
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

interface BridgeResult {
  requestId: string;
  success: boolean;
  error?: string;
  data?: unknown;
}

interface WorkspacesChild {
  sendAsyncMessage(name: string, data: unknown): void;
  receiveMessage(message: { name: string; data: unknown }): void;
  listBackups(): Promise<BridgeResult>;
  exportBackupBridge(target: object): void;
  didDestroy(): void;
}

type ChildConstructor = new () => WorkspacesChild;

interface SandboxApi {
  Sandbox(principal: string, options: Record<string, unknown>): object;
  evalInSandbox(code: string, sandbox: object): unknown;
  exportFunction(
    fn: (...args: unknown[]) => unknown,
    target: object,
    options: { defineAs: string },
  ): unknown;
  nukeSandbox(sandbox: object): void;
}

function loadChild(): ChildConstructor {
  const module = ChromeUtils.importESModule(
    "resource://noraneko/actors/NRWorkspacesChild.sys.mjs",
  ) as { NRWorkspacesChild: ChildConstructor };
  return module.NRWorkspacesChild;
}

function settleWithin<T>(
  promise: Promise<T>,
  ms: number,
): Promise<
  { state: "resolved"; value: T } | { state: "rejected"; error: unknown } | {
    state: "pending";
  }
> {
  return Promise.race([
    promise.then(
      (value) => ({ state: "resolved" as const, value }),
      (error: unknown) => ({ state: "rejected" as const, error }),
    ),
    new Promise<{ state: "pending" }>((resolve) =>
      setTimeout(() => resolve({ state: "pending" }), ms)
    ),
  ]);
}

/** A child whose messages are captured instead of crossing an IPC actor. */
function createChild(): {
  child: WorkspacesChild;
  sent: { name: string; data: { requestId?: string } }[];
} {
  const Child = loadChild();
  const child = new Child();
  const sent: { name: string; data: { requestId?: string } }[] = [];
  child.sendAsyncMessage = (name: string, data: unknown) => {
    sent.push({ name, data: data as { requestId?: string } });
  };
  return { child, sent };
}

async function testListRequestRoundTrip(): Promise<void> {
  const { child, sent } = createChild();
  try {
    const pending = child.listBackups();
    const early = await settleWithin(pending, 100);
    assert(
      early.state === "pending",
      `the request waits for the parent instead of failing: ${
        early.state === "rejected" ? String(early.error) : early.state
      }`,
    );
    assertEquals(sent.length, 1, "one message is sent to the parent");
    assertEquals(sent[0].name, "NRWorkspaces:ListBackups", "message name");
    const requestId = sent[0].data.requestId;
    assert(typeof requestId === "string", "request id is attached");
    child.receiveMessage({
      name: "NRWorkspaces:BackupResult",
      data: { requestId, success: true, data: [] },
    });
    const result = await pending;
    assert(result.success, "the parent result is delivered");
  } finally {
    child.didDestroy();
  }
}

async function testDestroySettlesPendingRequests(): Promise<void> {
  const { child } = createChild();
  const pending = child.listBackups();
  child.didDestroy();
  const outcome = await settleWithin(pending, 500);
  assert(
    outcome.state === "resolved" && !outcome.value.success &&
      outcome.value.error === "actor-destroyed",
    `didDestroy resolves the request: ${JSON.stringify(outcome)}`,
  );
}

async function testContentCanAwaitExportedBridge(): Promise<void> {
  const { child, sent } = createChild();
  const cu = Cu as unknown as SandboxApi;
  const sandbox = cu.Sandbox("https://example.com", {
    wantXrays: true,
    sandboxName: "QA g8 backup bridge",
  });
  try {
    // Same export as NRWorkspacesChild.actorCreated() does for the Hub page.
    child.exportBackupBridge(sandbox);
    cu.evalInSandbox(
      `globalThis.outcome = "pending";
       try {
         Promise.resolve(NRWorkspacesListBackups()).then(
           (r) => { globalThis.outcome = "ok:" + JSON.stringify(r); },
           (e) => { globalThis.outcome = "rejected:" + e; },
         );
       } catch (e) {
         globalThis.outcome = "threw:" + e;
       }`,
      sandbox,
    );
    const requestId = sent[0]?.data.requestId;
    if (requestId) {
      child.receiveMessage({
        name: "NRWorkspaces:BackupResult",
        data: { requestId, success: true, data: [] },
      });
    }
    let outcome = "";
    for (let i = 0; i < 20; i++) {
      await new Promise((resolve) => setTimeout(resolve, 25));
      outcome = String(cu.evalInSandbox("globalThis.outcome", sandbox));
      if (outcome !== "pending") break;
    }
    assert(
      outcome.startsWith("ok:") && outcome.includes('"success":true'),
      `a content page can await the bridge result: ${outcome}`,
    );
  } finally {
    child.didDestroy();
    cu.nukeSandbox(sandbox);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG8Bridge.test.ts", [
    { name: "child list request round trip", fn: testListRequestRoundTrip },
    {
      name: "didDestroy settles pending requests",
      fn: testDestroySettlesPendingRequests,
    },
    {
      name: "content can await the exported bridge (Hub page path)",
      fn: testContentCanAwaitExportedBridge,
    },
  ]);
}
