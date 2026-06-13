// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { SiteSpecificBrowserManager } from "../ssbManager.ts";
import { DataManager } from "../dataStore.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

/**
 * Test-local handle on the private `checkSiteCanBeInstall` method.
 *
 * Pulled off the prototype with a cast-to-unknown because the method is
 * `private` in TypeScript. Skipping `new SiteSpecificBrowserManager(...)`
 * because that constructor wires up `gBrowser.addTabsProgressListener` and
 * two `Services.obs.addObserver` calls — running it here would leak
 * listeners across the test session. `checkSiteCanBeInstall` does not read
 * `this`, so prototype-bound invocation is safe.
 */
const checkSiteCanBeInstall = (
  SiteSpecificBrowserManager.prototype as unknown as {
    checkSiteCanBeInstall: (uri: nsIURI) => boolean;
  }
).checkSiteCanBeInstall;

/** Convenience wrapper around `Services.io.newURI` for terse test bodies. */
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
      // Regression: before this fix, reading nsIURI.host on the
      // nsSimpleURI-family schemes (data:, blob:, moz-extension:,
      // javascript:) threw NS_ERROR_FAILURE, propagating up the tab
      // progress listener chain and breaking AMO extension installs on
      // Linux. `file:` is included as defensive coverage — `nsFileURL`
      // returns "" for `.host` rather than throwing, so it never tripped
      // the original bug, but pinning its return value here guards against
      // future regressions in the predicate's scheme handling.
      const specs = [
        "data:text/plain,hello",
        "blob:https://example.com/00000000-0000-0000-0000-000000000000",
        "moz-extension://00000000-0000-0000-0000-000000000000/popup.html",
        "javascript:void(0)",
        "file:///tmp/test.html",
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
  {
    name: "buildKey differentiates containers for the same start URL",
    fn() {
      const url = "https://example.com/";
      const keyA = DataManager.buildKey(url, 1);
      const keyB = DataManager.buildKey(url, 2);
      assert(keyA !== keyB, "composite keys must differ per container");
      assertEquals(
        DataManager.parseKey(keyA).userContextId,
        1,
        "container A id",
      );
      assertEquals(
        DataManager.parseKey(keyB).userContextId,
        2,
        "container B id",
      );
    },
  },
  {
    name: "buildKey default container uses userContextId 0",
    fn() {
      const url = "https://example.com/";
      const key = DataManager.buildKey(url, 0);
      assertEquals(
        DataManager.parseKey(key).userContextId,
        0,
        "default container id",
      );
      assertEquals(
        DataManager.parseKey(key).startUrl,
        url,
        "start URL preserved",
      );
    },
  },
];

/**
 * Entry point invoked by the colocated test runner
 * (`tools/src/colocated_test_runner.ts`). Runs every case in `tests`
 * sequentially and reports failures via the shared test harness.
 */
export async function runAllTests(): Promise<void> {
  await runTests("ssbManager.test.ts", tests);
}
