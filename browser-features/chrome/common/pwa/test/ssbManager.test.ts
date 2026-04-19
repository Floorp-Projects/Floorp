// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { SiteSpecificBrowserManager } from "../ssbManager.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// Access private checkSiteCanBeInstall via the prototype with a cast.
// Skipping `new SiteSpecificBrowserManager(...)` because the constructor
// adds tab progress listeners and observer notifications; the method
// under test does not use `this`, so prototype-bound invocation is safe.
const checkSiteCanBeInstall = (
  SiteSpecificBrowserManager.prototype as unknown as {
    checkSiteCanBeInstall: (uri: nsIURI) => boolean;
  }
).checkSiteCanBeInstall;

function makeURI(spec: string): nsIURI {
  return Services.io.newURI(spec);
}

const tests: TestCase[] = [
  {
    name: "https returns true",
    fn() {
      assertEquals(
        checkSiteCanBeInstall(makeURI("https://example.com/")),
        true,
        "https should be installable",
      );
    },
  },
  {
    name: "loopback http returns true",
    fn() {
      assertEquals(
        checkSiteCanBeInstall(makeURI("http://localhost/")),
        true,
        "http://localhost should be installable",
      );
      assertEquals(
        checkSiteCanBeInstall(makeURI("http://127.0.0.1/")),
        true,
        "http://127.0.0.1 should be installable",
      );
    },
  },
  {
    name: "non-loopback http returns false",
    fn() {
      assertEquals(
        checkSiteCanBeInstall(makeURI("http://example.com/")),
        false,
        "non-loopback http should not be installable",
      );
    },
  },
  {
    name: "about: and chrome: return false",
    fn() {
      assertEquals(
        checkSiteCanBeInstall(makeURI("about:blank")),
        false,
        "about: should not be installable",
      );
      assertEquals(
        checkSiteCanBeInstall(
          makeURI("chrome://browser/content/browser.xhtml"),
        ),
        false,
        "chrome: should not be installable",
      );
    },
  },
  {
    name: "hostless schemes return false without throwing",
    fn() {
      // Regression: before this fix, reading nsIURI.host on these schemes
      // threw NS_ERROR_FAILURE, propagating up the tab progress listener
      // chain and breaking AMO extension installs on Linux.
      const specs = [
        "data:text/plain,hello",
        "blob:https://example.com/00000000-0000-0000-0000-000000000000",
        "moz-extension://00000000-0000-0000-0000-000000000000/popup.html",
        "javascript:void(0)",
      ];
      for (const spec of specs) {
        let result: boolean | undefined;
        let threw: unknown;
        try {
          result = checkSiteCanBeInstall(makeURI(spec));
        } catch (e) {
          threw = e;
        }
        assertEquals(threw, undefined, `${spec} must not throw`);
        assertEquals(result, false, `${spec} must return false`);
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("ssbManager.test.ts", tests);
}
