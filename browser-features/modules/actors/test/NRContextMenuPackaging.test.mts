// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

interface ContextMenuActorModule {
  NRContextMenuChild?: unknown;
  isSecondaryContextMenuDocumentUri?: (uri: string) => boolean;
}

function testPackagedActorImportsFromResourceUri(): void {
  const actorModule = ChromeUtils.importESModule(
    "resource://noraneko/actors/NRContextMenuChild.sys.mjs",
  ) as unknown as ContextMenuActorModule;

  assert(
    typeof actorModule.NRContextMenuChild === "function",
    "the packaged Window Actor module and its bundled controller load in Gecko",
  );
  assert(
    typeof actorModule.isSecondaryContextMenuDocumentUri === "function",
    "the packaged actor exports its chrome-document guard",
  );
  assert(
    actorModule.isSecondaryContextMenuDocumentUri?.(
      "chrome://browser/content/webext-panels.xhtml",
    ) === true,
    "the packaged actor routes Firefox's WebExtension panel chrome document",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "packaged actor resource import",
      fn: testPackagedActorImportsFromResourceUri,
    },
  ];
  await runTests("NRContextMenuPackaging.test.mts", tests);
}
