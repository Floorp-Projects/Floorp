// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { createSettingsBridgeTransport } from "../../../src/lib/rpc/rpc.ts";

function fakeBridge(
  register: (callback: (data: string) => void) => void,
  send: (data: string) => void,
): Window {
  return {
    NRSettingsRegisterReceiveCallback: register,
    NRSettingsSend: send,
  } as unknown as Window;
}

async function captureRejection(promise: Promise<unknown>): Promise<unknown> {
  try {
    await promise;
    return null;
  } catch (error) {
    return error;
  }
}

async function testReceiverRegistersBeforeFirstSend(): Promise<void> {
  const events: string[] = [];
  let resolveBridge: ((bridge: Window) => void) | null = null;
  const bridgePromise = new Promise<Window>((resolve) => {
    resolveBridge = resolve;
  });
  const transport = createSettingsBridgeTransport(() => {
    events.push("resolve");
    return bridgePromise;
  });

  const registration = transport.on(() => undefined);
  const sending = transport.post("request");
  await Promise.resolve();
  assertEquals(
    events.join(","),
    "resolve",
    "no request is sent while receiver registration is pending",
  );

  assert(resolveBridge !== null, "the bridge resolver should be captured");
  resolveBridge(
    fakeBridge(
      () => events.push("register"),
      (data) => events.push(`send:${data}`),
    ),
  );
  await Promise.all([registration, sending]);
  assertEquals(
    events.join(","),
    "resolve,register,send:request",
    "the receiver is installed before birpc sends its first request",
  );
}

async function testRegistrationFailureCanRetryFromPost(): Promise<void> {
  const events: string[] = [];
  let registrationAttempts = 0;
  const bridge = fakeBridge(
    () => {
      registrationAttempts++;
      events.push(`register:${registrationAttempts}`);
      if (registrationAttempts === 1) {
        throw new Error("temporary registration failure");
      }
    },
    (data) => events.push(`send:${data}`),
  );
  const transport = createSettingsBridgeTransport(() =>
    Promise.resolve(bridge)
  );

  const firstError = await captureRejection(transport.on(() => undefined));
  assert(firstError instanceof Error, "the failed registration rejects");
  await transport.post("retry");
  assertEquals(
    events.join(","),
    "register:1,register:2,send:retry",
    "a later send re-registers the retained callback before retrying",
  );
}

const tests: TestCase[] = [
  {
    name: "settings bridge registers its receiver before the first send",
    fn: testReceiverRegistersBeforeFirstSend,
  },
  {
    name: "settings bridge retries registration after a failure",
    fn: testRegistrationFailureCanRetryFromPost,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("rpc.test.ts", tests);
}
