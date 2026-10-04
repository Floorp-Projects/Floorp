// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { runAllTests as rendererTests } from "../../../../libs/preact-xul/test/renderer.test.ts";
import { runAllTests as lifetimeTests } from "../../../../libs/preact-xul/test/lifetime.test.ts";

export async function runAllTests(): Promise<void> {
  await rendererTests();
  await lifetimeTests();
}
