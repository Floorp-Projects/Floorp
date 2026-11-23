// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

async function testFirstNavigationUsesCachedContainer(): Promise<void> {
  const { SsbRunnerUtils } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs",
  ) as typeof import("../SsbCommandLineHandler.sys.mts");
  const { Experiments } = ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  ) as typeof import("../../experiments/Experiments.sys.mts");
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
    "resource://gre/modules/Timer.sys.mjs",
  );
  const id = Services.uuid.generateUUID().toString();
  const url = `http://127.0.0.1:65534/pwa-initial-context-test/${
    id.replace(/[{}]/g, "")
  }`;
  const assignmentPref = "floorp.experiments.assignments.v1";
  const hadAssignmentPref = Services.prefs.prefHasUserValue(assignmentPref);
  const oldAssignmentPref = Services.prefs.getStringPref(assignmentPref, "{}");
  const oldAssignments = Experiments.assignments;
  const oldManifestAvailable = Experiments.manifestAvailable;
  const container = ContextualIdentityService.create(
    "PWA Initial Navigation Test",
    "fingerprint",
    "blue",
  );
  let resolveRequest: ((context: number) => void) | undefined;
  let observerRegistered = false;
  let timer: ReturnType<typeof setTimeout> | undefined;
  let openedWindow:
    | Awaited<ReturnType<typeof SsbRunnerUtils.openSsbWindow>>
    | undefined;
  const observer = {
    observe(subject: nsISupports) {
      const channel = subject.QueryInterface!(Ci.nsIHttpChannel);
      if (channel.URI.spec !== url) return;
      const requestContext = channel.loadInfo.originAttributes.userContextId ??
        0;
      // Observe the real Gecko initial request without contacting any server.
      channel.cancel(Cr.NS_BINDING_ABORTED);
      resolveRequest?.(requestContext);
    },
  };
  try {
    // Reproduce command-line startup before Experiments.init has populated its
    // in-memory assignments. Read the real persisted enrollment in that case.
    const assignments = { ...oldAssignments };
    delete assignments.pwa_container_support;
    Experiments.assignments = assignments;
    Experiments.manifestAvailable = false;
    Services.prefs.setStringPref(
      assignmentPref,
      JSON.stringify({
        pwa_container_support: {
          installId: "pwa-initial-navigation-test",
          variantId: "enabled",
          assignedAt: new Date().toISOString(),
        },
      }),
    );
    assertEquals(
      Experiments.getVariant("pwa_container_support"),
      null,
      "The experiment is not initialized in memory",
    );
    assertEquals(
      Experiments.getCachedEnrollment("pwa_container_support").variantId,
      "enabled",
      "Persisted enrollment is available before initialization",
    );

    const firstRequest = new Promise<number>((resolve) => {
      resolveRequest = resolve;
    });
    Services.obs.addObserver(observer, "http-on-modify-request");
    observerRegistered = true;
    openedWindow = await SsbRunnerUtils.openSsbWindow({
      id,
      name: "Initial Navigation Test",
      start_url: url,
      icon: "",
      userContextId: container.userContextId,
    });
    const timeout = new Promise<never>((_resolve, reject) => {
      timer = setTimeout(
        () => reject(new Error("No initial PWA HTTP request was observed")),
        15000,
      );
    });
    const initialContext = await Promise.race([firstRequest, timeout]);
    assertEquals(
      initialContext,
      container.userContextId,
      "The very first HTTP request uses the assigned container",
    );
    assert(
      openedWindow.gBrowser.selectedTab.getAttribute("usercontextid") ===
        String(container.userContextId),
      "The initial tab uses the same container",
    );
  } finally {
    if (timer !== undefined) clearTimeout(timer);
    if (observerRegistered) {
      Services.obs.removeObserver(observer, "http-on-modify-request");
    }
    openedWindow?.close();
    // Also close a window opened before promiseOpenWindow rejected.
    for (const window of Services.wm.getEnumerator("navigator:browser")) {
      try {
        const args = window.arguments;
        if (args?.[1]?.getPropertyAsAString("ssbid") === id) window.close();
      } catch {
        // Ordinary windows do not necessarily carry a PWA property bag.
      }
    }
    Experiments.assignments = oldAssignments;
    Experiments.manifestAvailable = oldManifestAvailable;
    if (hadAssignmentPref) {
      Services.prefs.setStringPref(assignmentPref, oldAssignmentPref);
    } else Services.prefs.clearUserPref(assignmentPref);
    ContextualIdentityService.remove(container.userContextId);
  }
}

async function testLinuxWindowsKeepDistinctLauncherClasses(): Promise<void> {
  const { SsbRunnerUtils } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs",
  ) as typeof import("../SsbCommandLineHandler.sys.mts");
  const { TaskbarExperiment } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs",
  ) as typeof import("../TaskbarExperiment.sys.mts");
  const oldGate = TaskbarExperiment.isEnabledForLinux();
  const apps = ["Alpha", "Beta"].map((name) => ({
    id: Services.uuid.generateUUID().toString(),
    name: `Window Class Test ${name}`,
    start_url: "about:blank",
    icon: "",
  }));
  const windows: Awaited<ReturnType<typeof SsbRunnerUtils.openSsbWindow>>[] =
    [];
  try {
    for (const app of apps) {
      const win = await SsbRunnerUtils.openSsbWindow(app);
      windows.push(win);
      assertEquals(
        win.document.documentElement.getAttribute("windowclass"),
        `floorp-${app.id.slice(1, -1)}`,
        "Each created PWA receives its generated desktop launcher identity",
      );
    }
    assert(
      windows[0].document.documentElement.getAttribute("windowclass") !==
        windows[1].document.documentElement.getAttribute("windowclass"),
      "Two PWA windows in one process keep different classes",
    );
    assertEquals(
      TaskbarExperiment.isEnabledForLinux(),
      oldGate,
      "Establishing window identity does not change the rollout gate",
    );
  } finally {
    for (const win of windows) win.close();
  }
}

async function testLinuxLegacyIdsKeepFallbackWindowClasses(): Promise<void> {
  const { SsbRunnerUtils } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs",
  ) as typeof import("../SsbCommandLineHandler.sys.mts");
  const cases = [
    {
      id: undefined,
      name: "Legacy Missing",
      expected: "floorp-legacy-missing",
    },
    { id: "{}", name: "Legacy Braces", expected: "floorp-legacy-braces" },
    { id: 123, name: "Legacy Number", expected: "floorp-legacy-number" },
    {
      id: { invalid: true },
      name: "Legacy Object",
      expected: "floorp-legacy-object",
    },
  ];
  for (const { id, name, expected } of cases) {
    // Old or malformed persisted JSON is only warning-validated by DataManager.
    const app = {
      id,
      name,
      start_url: "about:blank",
      icon: "",
    } as unknown as Parameters<typeof SsbRunnerUtils.openSsbWindow>[0];
    const win = await SsbRunnerUtils.openSsbWindow(app);
    try {
      assertEquals(
        win.document.documentElement.getAttribute("windowclass"),
        expected,
        "An unusable legacy id falls back to the existing name identity",
      );
    } finally {
      win.close();
    }
  }
}

async function testLinuxPwaStartupKeepsOrdinaryBrowserIdentity(): Promise<
  void
> {
  const { LinuxSupport } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/supports/Linux.sys.mjs",
  ) as typeof import("../supports/Linux.sys.mts");
  const { SsbRunnerUtils } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs",
  ) as typeof import("../SsbCommandLineHandler.sys.mts");
  const { BrowserWindowTracker } = ChromeUtils.importESModule(
    "resource:///modules/BrowserWindowTracker.sys.mjs",
  );
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  const category =
    "browser-window-before-initial-xul-layout-document-preparation";
  const entry = "resource://noraneko/modules/pwa/supports/Linux.sys.mjs";
  let previous: string | undefined;
  try {
    previous = Services.catMan.getCategoryEntry(category, entry);
  } catch {
    // Ordinary browser-first processes have no PWA startup registration.
  }
  const windows: { close(): void }[] = [];
  try {
    LinuxSupport.registerBrowserWindowIdentity();
    const args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    const url = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );
    url.data = "about:blank";
    args.appendElement(url);
    const browser = await BrowserWindowTracker.promiseOpenWindow({ args });
    windows.push(browser);
    const appInfo = Services.appinfo as typeof Services.appinfo & {
      readonly remotingName?: string;
    };
    assertEquals(
      browser.document.documentElement.getAttribute("windowclass"),
      appInfo.remotingName || AppConstants.MOZ_APP_NAME,
      "The pre-layout category gives ordinary windows the browser identity",
    );
    const app = {
      id: Services.uuid.generateUUID().toString(),
      name: "Browser Identity Regression",
      start_url: "about:blank",
      icon: "",
    };
    const pwa = await SsbRunnerUtils.openSsbWindow(app);
    windows.push(pwa);
    assertEquals(
      pwa.document.documentElement.getAttribute("windowclass"),
      LinuxSupport.getWindowClass(app),
      "Browser identity preparation preserves the PWA launcher identity",
    );
  } finally {
    for (const win of windows) win.close();
    if (previous === undefined) {
      Services.catMan.deleteCategoryEntry(category, entry, false);
    } else {
      Services.catMan.addCategoryEntry(category, entry, previous, false, true);
    }
  }
}

export async function runAllTests(): Promise<void> {
  if (Services.appinfo.OS !== "Linux") {
    console.info(
      "[SsbCommandLineHandler.test] Initial Linux container navigation requires Linux; skipped",
    );
    return;
  }
  await runTests("SsbCommandLineHandler.test.mts", [{
    name:
      "the first Linux PWA request uses its container before experiment initialization",
    fn: testFirstNavigationUsesCachedContainer,
  }, {
    name: "Linux PWA windows keep their distinct generated launcher classes",
    fn: testLinuxWindowsKeepDistinctLauncherClasses,
  }, {
    name:
      "Linux PWA legacy ids retain fallback classes without failing startup",
    fn: testLinuxLegacyIdsKeepFallbackWindowClasses,
  }, {
    name:
      "Linux PWA startup keeps ordinary browser and PWA identities separate",
    fn: testLinuxPwaStartupKeepsOrdinaryBrowserIdentity,
  }]);
}
