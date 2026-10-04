// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { acquireContextMenuStyle } from "../style.ts";

const XHTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
const XUL_NAMESPACE =
  "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function testXmlFallbackUsesXhtmlStyleAndReferenceCounting(): void {
  const xmlDocument = document.implementation.createDocument(
    XUL_NAMESPACE,
    "window",
  );
  const firstLease = acquireContextMenuStyle(xmlDocument);
  const secondLease = acquireContextMenuStyle(xmlDocument);
  const style = xmlDocument.querySelector(
    "[data-floorp-context-menu-style]",
  );

  assert(style !== null, "an XML document receives the non-Gecko fallback");
  assertEquals(
    style.namespaceURI,
    XHTML_NAMESPACE,
    "the fallback is an XHTML style element, never a null-namespace XUL node",
  );

  firstLease.release();
  assert(
    style.isConnected,
    "releasing one of two leases keeps the shared stylesheet",
  );
  secondLease.release();
  assert(
    !style.isConnected,
    "the final lease removes the owned fallback stylesheet",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name: "XML fallback stylesheet namespace and lease semantics",
    fn: testXmlFallbackUsesXhtmlStyleAndReferenceCounting,
  }];
  await runTests("contextMenuStyle.test.ts", tests);
}
