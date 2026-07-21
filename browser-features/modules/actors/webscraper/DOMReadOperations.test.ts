// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import type { DOMOpsDeps } from "./DOMDeps.ts";
import { DOMReadOperations } from "./DOMReadOperations.ts";

function createReadOperations(doc: Document): DOMReadOperations {
  const deps: DOMOpsDeps = {
    context: {} as DOMOpsDeps["context"],
    highlightManager: {} as DOMOpsDeps["highlightManager"],
    eventDispatcher: {} as DOMOpsDeps["eventDispatcher"],
    translationHelper: {} as DOMOpsDeps["translationHelper"],
    getContentWindow: () => null,
    getDocument: () => doc,
  };
  return new DOMReadOperations(deps);
}

function buildScopedIframeFixture(): Document {
  const doc = document.implementation.createHTMLDocument("scoped text test");
  const marker = doc.createElement("main");
  marker.id = "scope";
  marker.textContent = "Text inside the selected marker.";
  doc.body.appendChild(marker);

  const iframe = doc.createElement("iframe");
  const iframeDoc = document.implementation.createHTMLDocument(
    "outside iframe",
  );
  iframeDoc.body.textContent = "Text from an iframe outside the marker.";
  Object.defineProperty(iframe, "contentDocument", { value: iframeDoc });
  doc.body.appendChild(iframe);
  return doc;
}

const tests: TestCase[] = [
  {
    name: "scoped text can exclude document-wide iframe content",
    fn() {
      const operations = createReadOperations(buildScopedIframeFixture());
      const withoutIframes = operations.getText({
        mode: "scoped",
        selector: "#scope",
        enableFingerprints: false,
        includeIframes: false,
      });
      const withIframes = operations.getText({
        mode: "scoped",
        selector: "#scope",
        enableFingerprints: false,
        includeIframes: true,
      });

      assert(
        withoutIframes?.includes("Text inside the selected marker.") ?? false,
        "scoped extraction should retain marker text",
      );
      assert(
        !withoutIframes?.includes("Text from an iframe outside the marker."),
        "includeIframes=false should exclude document-wide iframe text",
      );
      assert(
        withIframes?.includes("Text from an iframe outside the marker.") ??
          false,
        "the default-compatible iframe path should remain available",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("DOMReadOperations.test.ts", tests);
}
