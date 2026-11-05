// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { NRProgressiveWebAppParent } from "../NRProgressiveWebAppParent.sys.mts";
import {
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";

function testOnlyCurrentTopDocumentNotifies(): void {
  const topic = "nora-pwa-check-page-has-manifest";
  const browserWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as { gBrowser: { selectedBrowser: nsISupports } };
  const browser = browserWindow.gBrowser.selectedBrowser;
  const activeDocument = {};
  type Context = {
    top?: Context;
    currentWindowGlobal: object;
    embedderElement: typeof browser | null;
  };
  const top: Context = {
    currentWindowGlobal: activeDocument,
    embedderElement: browser,
  };
  top.top = top;
  const child: Context = {
    top,
    currentWindowGlobal: activeDocument,
    embedderElement: browser,
  };
  const subjects: (nsISupports | null)[] = [];
  const observer = (subject: nsISupports | null) => subjects.push(subject);
  const notify = (context: Context, manager: object) =>
    NRProgressiveWebAppParent.prototype.sendNotifyToPageAction.call(
      {
        browsingContext: context,
        manager,
      } as unknown as NRProgressiveWebAppParent,
    );
  Services.obs.addObserver(observer, topic);
  try {
    notify(child, activeDocument);
    assertEquals(
      subjects.length,
      0,
      "subframe pageshow cannot refresh the top page",
    );
    notify(top, {});
    assertEquals(
      subjects.length,
      0,
      "late messages from a replaced document are ignored",
    );
    notify(top, activeDocument);
    assertEquals(subjects.length, 1, "current top-level document refreshes");
    assertEquals(
      subjects[0],
      browser,
      "notification identifies the originating browser",
    );
    top.embedderElement = null;
    notify(top, activeDocument);
    assertEquals(
      subjects.length,
      1,
      "a detached embedder cannot broadcast globally",
    );
  } finally {
    Services.obs.removeObserver(observer, topic);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("NRProgressiveWebAppParent.test.mts", [{
    name: "manifest notifications retain current top-document ownership",
    fn: testOnlyCurrentTopDocumentNotifies,
  }]);
}
