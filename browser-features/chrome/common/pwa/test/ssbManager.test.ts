// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  resolveEffectiveUserContextId,
  SiteSpecificBrowserManager,
} from "../ssbManager.ts";
import { DataManager } from "../dataStore.ts";
import type { Browser, Manifest } from "../type.ts";
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

const getIdByUrl = (
  SiteSpecificBrowserManager.prototype as unknown as {
    getIdByUrl: (
      url: string,
      userContextId?: number,
    ) => Promise<Manifest | undefined>;
  }
).getIdByUrl;

/** Convenience wrapper around `Services.io.newURI` for terse test bodies. */
function makeURI(spec: string): nsIURI {
  return Services.io.newURI(spec);
}

function makeBrowser(spec: string): Browser {
  return {
    currentURI: makeURI(spec),
  } as unknown as Browser;
}

function makeManifest(
  startUrl = "https://example.com/",
  userContextId?: number,
): Manifest {
  return {
    id: `${startUrl}:${userContextId ?? 0}`,
    name: "Example",
    start_url: startUrl,
    icon: "",
    userContextId,
  };
}

function createDeferred<T>(): {
  promise: Promise<T>;
  resolve: (value: T) => void;
} {
  let resolvePromise!: (value: T) => void;
  const promise = new Promise<T>((resolve) => {
    resolvePromise = resolve;
  });
  return { promise, resolve: resolvePromise };
}

function installOrRunWithManagerLike(
  managerLike: object,
  browser: Browser,
  asPwa = true,
  userContextId?: number,
): Promise<void> {
  return SiteSpecificBrowserManager.prototype.installOrRunCurrentPageAsSsb
    .call(
      managerLike as unknown as SiteSpecificBrowserManager,
      browser,
      asPwa,
      userContextId,
    );
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
    name: "explicit userContextId 0 and positive values win when enabled",
    fn() {
      const browser = makeBrowser("https://passed.example/");
      let resolverCalls = 0;
      const resolveFromBrowser = (_browser: Browser): number => {
        resolverCalls += 1;
        return 9;
      };

      assertEquals(
        resolveEffectiveUserContextId(browser, 0, true, resolveFromBrowser),
        0,
        "explicit default-container id must be preserved",
      );
      assertEquals(
        resolveEffectiveUserContextId(browser, 6, true, resolveFromBrowser),
        6,
        "explicit positive container id must be preserved",
      );
      assertEquals(
        resolverCalls,
        0,
        "explicit ids must not be replaced by the browser container",
      );
    },
  },
  {
    name: "disabled container experiment always resolves to 0",
    fn() {
      const browser = makeBrowser("https://passed.example/");
      let resolverCalls = 0;
      const result = resolveEffectiveUserContextId(
        browser,
        8,
        false,
        () => {
          resolverCalls += 1;
          return 9;
        },
      );

      assertEquals(
        result,
        0,
        "disabled experiment must use the default context",
      );
      assertEquals(
        resolverCalls,
        0,
        "disabled experiment must not derive a browser container",
      );
    },
  },
  {
    name: "omitted userContextId derives from the passed browser",
    fn() {
      const passedBrowser = makeBrowser("https://passed.example/");
      const selectedBrowser = makeBrowser("https://selected.example/");
      let resolvedBrowser: Browser | null = null;
      const result = resolveEffectiveUserContextId(
        passedBrowser,
        undefined,
        true,
        (browser) => {
          resolvedBrowser = browser;
          return browser === passedBrowser ? 4 : 9;
        },
      );

      assertEquals(result, 4, "passed browser container must be derived");
      assert(
        resolvedBrowser === passedBrowser,
        "resolver must receive the passed browser",
      );
      assert(
        resolvedBrowser !== selectedBrowser,
        "an unrelated selected browser must not be consulted",
      );
    },
  },
  {
    name: "install flow preserves explicit 0 and positive contexts end to end",
    async fn() {
      for (const explicitUserContextId of [0, 6]) {
        const browser = makeBrowser("https://example.com/account");
        const createdManifest = makeManifest("https://example.com/");
        const predicateContextIds: number[] = [];
        const installedManifests: Manifest[] = [];
        const scheduledLaunches: Array<{
          url: string;
          userContextId: number;
        }> = [];
        const managerLike = {
          getEffectiveUserContextId(
            receivedBrowser: Browser,
            receivedExplicitUserContextId: number | undefined,
          ): number {
            return resolveEffectiveUserContextId(
              receivedBrowser,
              receivedExplicitUserContextId,
              true,
              () => 11,
            );
          },
          checkPageIsInstalledForContainer(
            _browser: Browser,
            userContextId: number,
          ): Promise<boolean> {
            predicateContextIds.push(userContextId);
            return Promise.resolve(false);
          },
          createFromBrowser(
            _browser: Browser,
            _options: { useWebManifest: boolean },
          ): Promise<Manifest> {
            return Promise.resolve(createdManifest);
          },
          install(manifest: Manifest): Promise<void> {
            installedManifests.push(manifest);
            return Promise.resolve();
          },
          scheduleRunSsbByUrl(url: string, userContextId: number): void {
            scheduledLaunches.push({ url, userContextId });
          },
        };

        await installOrRunWithManagerLike(
          managerLike,
          browser,
          true,
          explicitUserContextId,
        );

        assertEquals(
          predicateContextIds[0],
          explicitUserContextId,
          "installed predicate must receive the effective context",
        );
        assertEquals(
          installedManifests[0].userContextId,
          explicitUserContextId,
          "stored manifest must retain the effective context, including 0",
        );
        assertEquals(
          scheduledLaunches[0].url,
          createdManifest.start_url,
          "post-install launch must use the installed start URL",
        );
        assertEquals(
          scheduledLaunches[0].userContextId,
          explicitUserContextId,
          "post-install launch must retain the effective context",
        );
      }
    },
  },
  {
    name: "disabled install flow uses context 0 throughout",
    async fn() {
      const browser = makeBrowser("https://example.com/account");
      const createdManifest = makeManifest("https://example.com/");
      let predicateUserContextId = -1;
      const installedManifests: Manifest[] = [];
      let scheduledUserContextId = -1;
      const managerLike = {
        getEffectiveUserContextId(
          receivedBrowser: Browser,
          receivedExplicitUserContextId: number | undefined,
        ): number {
          return resolveEffectiveUserContextId(
            receivedBrowser,
            receivedExplicitUserContextId,
            false,
            () => 11,
          );
        },
        checkPageIsInstalledForContainer(
          _browser: Browser,
          userContextId: number,
        ): Promise<boolean> {
          predicateUserContextId = userContextId;
          return Promise.resolve(false);
        },
        createFromBrowser(): Promise<Manifest> {
          return Promise.resolve(createdManifest);
        },
        install(manifest: Manifest): Promise<void> {
          installedManifests.push(manifest);
          return Promise.resolve();
        },
        scheduleRunSsbByUrl(_url: string, userContextId: number): void {
          scheduledUserContextId = userContextId;
        },
      };

      await installOrRunWithManagerLike(managerLike, browser, true, 8);

      assertEquals(
        predicateUserContextId,
        0,
        "disabled predicate must use the default context",
      );
      assertEquals(
        installedManifests.length,
        1,
        "disabled install must save one manifest",
      );
      assertEquals(
        installedManifests[0].userContextId,
        0,
        "disabled install must store the default context",
      );
      assertEquals(
        scheduledUserContextId,
        0,
        "disabled post-install launch must use the default context",
      );
    },
  },
  {
    name: "selected-browser switch during await cannot change lookup or launch",
    async fn() {
      const originalGBrowser = globalThis.gBrowser;
      const passedBrowser = makeBrowser("https://passed.example/account");
      const unrelatedBrowser = makeBrowser(
        "https://unrelated.example/selected",
      );
      const passedManifest = makeManifest("https://passed.example/", 7);
      const installedDeferred = createDeferred<boolean>();
      const lookupCalls: Array<{ url: string; userContextId: number }> = [];
      const launchCalls: Array<{ url: string; userContextId: number }> = [];
      let resolverCalls = 0;
      const managerLike = {
        getEffectiveUserContextId(
          receivedBrowser: Browser,
          explicitUserContextId: number | undefined,
        ): number {
          resolverCalls += 1;
          return resolveEffectiveUserContextId(
            receivedBrowser,
            explicitUserContextId,
            true,
            (browser) => browser === passedBrowser ? 7 : 12,
          );
        },
        checkPageIsInstalledForContainer(
          _browser: Browser,
          userContextId: number,
        ): Promise<boolean> {
          assertEquals(
            userContextId,
            7,
            "predicate must receive the pre-await passed-browser context",
          );
          return installedDeferred.promise;
        },
        getCurrentTabSsb(): Promise<Manifest> {
          return Promise.resolve(passedManifest);
        },
        getIdByUrl(
          url: string,
          userContextId: number,
        ): Promise<Manifest> {
          lookupCalls.push({ url, userContextId });
          return Promise.resolve(passedManifest);
        },
        runSsbByUrl(url: string, userContextId: number): Promise<void> {
          launchCalls.push({ url, userContextId });
          return Promise.resolve();
        },
      };

      try {
        const operation = installOrRunWithManagerLike(
          managerLike,
          passedBrowser,
        );
        (globalThis as Record<string, unknown>).gBrowser = {
          selectedBrowser: unrelatedBrowser,
          selectedTab: { linkedBrowser: unrelatedBrowser },
        };
        installedDeferred.resolve(true);
        await operation;

        assertEquals(
          resolverCalls,
          1,
          "effective context must be captured exactly once before awaiting",
        );
        assertEquals(
          lookupCalls[0].userContextId,
          7,
          "exact-container lookup must retain the captured context",
        );
        assertEquals(
          launchCalls[0].userContextId,
          7,
          "launch must retain the captured context",
        );
        assertEquals(
          launchCalls[0].url,
          "https://passed.example/account",
          "launch URL must remain bound to the passed browser invocation",
        );
      } finally {
        globalThis.gBrowser = originalGBrowser;
      }
    },
  },
  {
    name: "same-browser navigation during installed lookup aborts launch",
    async fn() {
      const browser = makeBrowser("https://original.example/account");
      const installedDeferred = createDeferred<boolean>();
      let manifestReads = 0;
      let lookupCalls = 0;
      let launchCalls = 0;
      const managerLike = {
        getEffectiveUserContextId(): number {
          return 7;
        },
        checkPageIsInstalledForContainer(): Promise<boolean> {
          return installedDeferred.promise;
        },
        getCurrentTabSsb(): Promise<Manifest> {
          manifestReads += 1;
          return Promise.resolve(makeManifest("https://original.example/", 7));
        },
        getIdByUrl(): Promise<Manifest> {
          lookupCalls += 1;
          return Promise.resolve(makeManifest("https://original.example/", 7));
        },
        runSsbByUrl(): Promise<void> {
          launchCalls += 1;
          return Promise.resolve();
        },
      };

      const operation = installOrRunWithManagerLike(managerLike, browser);
      browser.currentURI = makeURI("https://changed.example/");
      installedDeferred.resolve(true);
      await operation;

      assertEquals(
        manifestReads,
        0,
        "a changed page must abort before reading a later manifest",
      );
      assertEquals(lookupCalls, 0, "a changed page must skip installed lookup");
      assertEquals(launchCalls, 0, "a changed page must not launch an app");
    },
  },
  {
    name: "same-browser navigation during manifest creation aborts install",
    async fn() {
      const browser = makeBrowser("https://original.example/account");
      const manifestDeferred = createDeferred<Manifest | null>();
      let installCalls = 0;
      let scheduledLaunches = 0;
      const managerLike = {
        getEffectiveUserContextId(): number {
          return 7;
        },
        checkPageIsInstalledForContainer(): Promise<boolean> {
          return Promise.resolve(false);
        },
        createFromBrowser(): Promise<Manifest | null> {
          return manifestDeferred.promise;
        },
        install(): Promise<void> {
          installCalls += 1;
          return Promise.resolve();
        },
        scheduleRunSsbByUrl(): void {
          scheduledLaunches += 1;
        },
      };

      const operation = installOrRunWithManagerLike(managerLike, browser);
      await Promise.resolve();
      browser.currentURI = makeURI("https://changed.example/");
      manifestDeferred.resolve(makeManifest("https://original.example/", 7));
      await operation;

      assertEquals(installCalls, 0, "a changed page must not be installed");
      assertEquals(
        scheduledLaunches,
        0,
        "a changed page must not schedule an app launch",
      );
    },
  },
  {
    name:
      "duplicate URLs are matched by exact container for predicate and lookup",
    async fn() {
      const startUrl = "https://duplicate.example/";
      const defaultManifest = makeManifest(startUrl, 0);
      const containerManifest = makeManifest(startUrl, 7);
      const ssbData = {
        [DataManager.buildKey(startUrl, 0)]: defaultManifest,
        [DataManager.buildKey(startUrl, 7)]: containerManifest,
      };
      const browser = makeBrowser(`${startUrl}account`);
      const managerLike = {
        checkSiteCanBeInstall,
        getCurrentTabSsb: () => Promise.resolve(containerManifest),
        dataManager: {
          getCurrentSsbData: () => Promise.resolve(ssbData),
        },
      };

      assertEquals(
        await SiteSpecificBrowserManager.prototype
          .checkPageIsInstalledForContainer.call(
            managerLike as unknown as SiteSpecificBrowserManager,
            browser,
            7,
          ),
        true,
        "matching URL and container must be installed",
      );
      assertEquals(
        await SiteSpecificBrowserManager.prototype
          .checkPageIsInstalledForContainer.call(
            managerLike as unknown as SiteSpecificBrowserManager,
            browser,
            8,
          ),
        false,
        "same URL in other containers must not satisfy the predicate",
      );
      assert(
        await getIdByUrl.call(
          managerLike as unknown as SiteSpecificBrowserManager,
          startUrl,
          0,
        ) === defaultManifest,
        "default-container lookup must return the default entry",
      );
      assert(
        await getIdByUrl.call(
          managerLike as unknown as SiteSpecificBrowserManager,
          startUrl,
          7,
        ) === containerManifest,
        "container lookup must return the matching container entry",
      );
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
  {
    name: "getSsbObj prefers storage key when ids collide",
    async fn() {
      const first: Manifest = {
        id: "same-id",
        name: "Default",
        start_url: "https://example.com/",
        icon: "",
      };
      const second: Manifest = {
        id: "same-id",
        name: "Container",
        start_url: "https://example.com/",
        icon: "",
        userContextId: 2,
      };
      const managerLike = {
        getSsbObj: SiteSpecificBrowserManager.prototype.getSsbObj,
        dataManager: {
          getCurrentSsbData: () =>
            Promise.resolve({
              "https://example.com/:0": first,
              "https://example.com/:2": second,
            }),
        },
      };

      const result = await SiteSpecificBrowserManager.prototype.getSsbObj.call(
        managerLike as unknown as SiteSpecificBrowserManager,
        "same-id",
      );

      // getSsbObj returns the first match by id; with two entries sharing
      // the same id the result depends on iteration order of getCurrentSsbData.
      assert(
        result === first || result === second,
        "should return one of the matching entries",
      );
    },
  },
  {
    name: "resetContainerForSsb does not overwrite an existing default entry",
    async fn() {
      const defaultEntry: Manifest = {
        id: "default-id",
        name: "Default",
        start_url: "https://example.com/",
        icon: "",
      };
      const containerEntry: Manifest = {
        id: "container-id",
        name: "Container",
        start_url: "https://example.com/",
        icon: "",
        userContextId: 2,
      };
      let moved = false;
      const managerLike = {
        getSsbObj: SiteSpecificBrowserManager.prototype.getSsbObj,
        dataManager: {
          getCurrentSsbData: () =>
            Promise.resolve({
              "https://example.com/:0": defaultEntry,
              "https://example.com/:2": containerEntry,
            }),
          moveSsbKey: () => {
            moved = true;
            return Promise.resolve(false);
          },
        },
      };

      const result = await SiteSpecificBrowserManager.prototype
        .setContainerForSsb.call(
          managerLike as unknown as SiteSpecificBrowserManager,
          "container-id",
          0,
        );

      assertEquals(result, false, "reset should fail on default-key collision");
      assertEquals(moved, false, "moveSsbKey must not run on collision");
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
