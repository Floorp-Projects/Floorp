// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { runAllTests as rendererTests } from "#libs/preact-xul/test/renderer.test.ts";
import { runAllTests as lifetimeTests } from "#libs/preact-xul/test/lifetime.test.ts";
import { runAllTests as adversarialTests } from "#libs/preact-xul/test/adversarial.test.ts";
import { runAllTests as productionUnloadTests } from "#libs/preact-xul/test/productionUnload.test.ts";

export async function runAllTests(): Promise<void> {
  await rendererTests();
  await lifetimeTests();
  await adversarialTests();
  await productionUnloadTests();
}
