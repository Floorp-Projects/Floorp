// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  installOffscreenPolyfill,
  type OffscreenDocumentOptions,
  OffscreenPolyfill,
} from "../../../polyfills/offscreen/OffscreenPolyfill.sys.mts";
import { OFFSCREEN_POLYFILL_SOURCE } from "../../../polyfills/offscreen/OffscreenPolyfillSource.sys.mts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";

export async function runAllTests(): Promise<void> {
  await runTests("OffscreenPolyfill.test.mts", [
    {
      name: "legacy bridge delegates to the native host without creating DOM",
      async fn() {
        const options: OffscreenDocumentOptions = {
          url: "offscreen.html",
          reasons: ["DOM_PARSER"],
          justification: "Verify legacy entry point",
        };
        let exists = false;
        let received: OffscreenDocumentOptions | undefined;
        const bridge = new OffscreenPolyfill({
          nativeAPI: {
            createDocument(value) {
              received = value;
              exists = true;
              return Promise.resolve();
            },
            hasDocument() {
              return Promise.resolve(exists);
            },
            closeDocument() {
              exists = false;
              return Promise.resolve();
            },
          },
        });
        assertEquals(await bridge.hasDocument(), false, "initial native state");
        await bridge.createDocument(options);
        assertEquals(received, options, "native options preserved");
        assertEquals(await bridge.hasDocument(), true, "native host reported");
        await bridge.closeDocument();
        assertEquals(await bridge.hasDocument(), false, "native host closed");
      },
    },
    {
      name: "retired installer never mutates extension API bindings",
      fn() {
        assertEquals(installOffscreenPolyfill(), false, "no shim installed");
        assertEquals(
          installOffscreenPolyfill({ force: true }),
          false,
          "force cannot replace native bindings",
        );
        assert(
          !OFFSCREEN_POLYFILL_SOURCE.includes("createElement") &&
            !OFFSCREEN_POLYFILL_SOURCE.includes("chrome.offscreen ="),
          "legacy converter source cannot create iframe hosts or replace APIs",
        );
      },
    },
  ]);
}
