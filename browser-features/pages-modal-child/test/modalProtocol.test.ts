// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";
import {
  applyModalInit,
  applyModalRemove,
  createModalCancelMessage,
  createModalRemoveMessage,
  createModalSubmitMessage,
  isNoraModalInitMessage,
  isNoraModalRemoveMessage,
  isNoraModalSubmitMessage,
  matchesModalRequest,
  type ModalPageState,
  shouldAcceptModalInit,
} from "../src/modalProtocol.ts";

const form = {
  forms: [{ id: "name", type: "text" as const, value: "Floorp" }],
  title: "Protocol",
};

function init(requestId: string, epoch: number) {
  return { type: "nora-modal-init" as const, requestId, epoch, form };
}

function testEnvelopeValidationFailsClosed(): void {
  assert(isNoraModalInitMessage(init("valid", 1)), "valid init is accepted");
  assertEquals(
    isNoraModalInitMessage({ type: "nora-modal-init", form }),
    false,
    "untagged init is rejected",
  );
  assertEquals(
    isNoraModalSubmitMessage({
      type: "nora-modal-submit",
      requestId: "valid",
      epoch: 1,
      reason: "cancel",
      result: { not: "null" },
    }),
    false,
    "cancel must carry null",
  );
  assertEquals(
    isNoraModalRemoveMessage({
      type: "nora-modal-remove",
      requestId: "valid",
      epoch: 1,
      reason: "remove",
    }),
    false,
    "remove must carry explicit null",
  );
}

function testCurrentAndNewerInitOnly(): void {
  const current = applyModalInit(null, init("current", 2));
  assert(current !== null, "first valid init creates state");
  assert(
    shouldAcceptModalInit(current, init("current", 2)),
    "same request is idempotently accepted",
  );
  assertEquals(
    shouldAcceptModalInit(current, init("different", 2)),
    false,
    "same epoch cannot change identity",
  );
  assertEquals(
    applyModalInit(current, init("older", 1)),
    current,
    "older init is ignored",
  );
  const newer = applyModalInit(current, init("newer", 3));
  assertEquals(newer?.requestId, "newer", "newer epoch replaces current");
}

function testStaleRemoveCannotClearNewerState(): void {
  const current = applyModalInit(null, init("current", 4));
  assert(current !== null, "current state exists");
  const staleRemove = createModalRemoveMessage(
    { requestId: "stale", epoch: 3 },
    "replacement",
  );
  assertEquals(
    applyModalRemove(current, staleRemove),
    current,
    "stale remove preserves current state",
  );
  assertEquals(
    applyModalRemove(
      current,
      createModalRemoveMessage(current, "cancel"),
    ),
    null,
    "matching remove clears state",
  );
}

function testIdentityEchoForSubmitAndCancel(): void {
  const state: ModalPageState = {
    requestId: "echo",
    epoch: 5,
    form,
  };
  const submit = createModalSubmitMessage(state, { name: "Floorp" });
  const cancel = createModalCancelMessage(state);
  assert(isNoraModalSubmitMessage(submit), "built submit validates");
  assert(isNoraModalSubmitMessage(cancel), "built cancel validates");
  assert(
    matchesModalRequest(state, submit),
    "submit echoes rendered identity",
  );
  assert(
    matchesModalRequest(state, cancel),
    "cancel echoes rendered identity",
  );
  assertEquals(submit.result?.name, "Floorp", "submit payload is preserved");
  assertEquals(cancel.result, null, "cancel payload is null");
}

function testStaleSubmitDoesNotMatchCurrent(): void {
  const current = applyModalInit(null, init("current", 7));
  assert(current !== null, "current state exists");
  const stale = createModalSubmitMessage(
    { requestId: "old", epoch: 6 },
    { stale: "value" },
  );
  assertEquals(
    matchesModalRequest(current, stale),
    false,
    "stale submit cannot target newer form",
  );
}

const tests: TestCase[] = [
  {
    name: "envelope validation fails closed",
    fn: testEnvelopeValidationFailsClosed,
  },
  { name: "current and newer init only", fn: testCurrentAndNewerInitOnly },
  {
    name: "stale remove cannot clear newer state",
    fn: testStaleRemoveCannotClearNewerState,
  },
  {
    name: "identity echo for submit and cancel",
    fn: testIdentityEchoForSubmitAndCancel,
  },
  {
    name: "stale submit does not match current",
    fn: testStaleSubmitDoesNotMatchCurrent,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("modalProtocol.test.ts", tests);
}
