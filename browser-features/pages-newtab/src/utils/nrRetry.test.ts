// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { callNRWithRetry } from "./nrRetry.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

async function testImmediateSuccess(): Promise<void> {
  const result = await callNRWithRetry(
    (cb) => cb('{"ok":true}'),
    (data) => JSON.parse(data) as { ok: boolean },
    { retries: 3, timeoutMs: 500 },
  );
  assertEquals(result.ok, true, "should return parsed result on success");
}

async function testRetryOnParseError(): Promise<void> {
  let attempt = 0;
  const result = await callNRWithRetry(
    (cb) => {
      attempt++;
      if (attempt < 3) {
        cb("bad-json");
      } else {
        cb('{"value":42}');
      }
    },
    (data) => JSON.parse(data) as { value: number },
    { retries: 5, timeoutMs: 500, delayMs: 10 },
  );
  assertEquals(result.value, 42, "should succeed after retries");
  assertEquals(attempt, 3, "should have attempted 3 times");
}

async function testShouldRetryCondition(): Promise<void> {
  let attempt = 0;
  const result = await callNRWithRetry(
    (cb) => {
      attempt++;
      cb(JSON.stringify({ ready: attempt >= 3 }));
    },
    (data) => JSON.parse(data) as { ready: boolean },
    {
      retries: 5,
      timeoutMs: 500,
      delayMs: 10,
      shouldRetry: (parsed) => !parsed.ready,
    },
  );
  assert(result.ready, "should eventually get ready=true");
}

async function testTimeoutThrows(): Promise<void> {
  try {
    await callNRWithRetry(
      () => {
        // Never call the callback → timeout
      },
      (data) => data,
      { retries: 1, timeoutMs: 50, delayMs: 10 },
    );
    throw new Error("should have thrown");
  } catch (error) {
    assert(
      error instanceof Error && error.message.includes("timed out"),
      "should throw timeout error",
    );
  }
}

async function testExhaustedRetriesThrows(): Promise<void> {
  try {
    await callNRWithRetry(
      (cb) => cb("not-json"),
      (data) => JSON.parse(data),
      { retries: 2, timeoutMs: 500, delayMs: 10 },
    );
    throw new Error("should have thrown");
  } catch (error) {
    assert(error instanceof Error, "should throw after exhausted retries");
  }
}

async function testInvokerThrows(): Promise<void> {
  try {
    await callNRWithRetry(
      () => {
        throw new Error("invoker error");
      },
      (data) => data,
      { retries: 1, timeoutMs: 500, delayMs: 10 },
    );
    throw new Error("should have thrown");
  } catch (error) {
    assert(
      error instanceof Error && error.message.includes("invoker error"),
      "should propagate invoker error",
    );
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "immediate success", fn: testImmediateSuccess },
    { name: "retry on parse error", fn: testRetryOnParseError },
    { name: "shouldRetry condition", fn: testShouldRetryCondition },
    { name: "timeout throws", fn: testTimeoutThrows },
    { name: "exhausted retries throws", fn: testExhaustedRetriesThrows },
    { name: "invoker throws", fn: testInvokerThrows },
  ];

  await runTests("nrRetry.test.ts", tests);
}
