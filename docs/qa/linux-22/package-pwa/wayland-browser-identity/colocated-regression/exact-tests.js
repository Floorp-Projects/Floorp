var qaPwaTests = (() => {
  var __defProp = Object.defineProperty;
  var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
  var __getOwnPropNames = Object.getOwnPropertyNames;
  var __hasOwnProp = Object.prototype.hasOwnProperty;
  var __export = (target, all) => {
    for (var name in all)
      __defProp(target, name, { get: all[name], enumerable: true });
  };
  var __copyProps = (to, from, except, desc) => {
    if (from && typeof from === "object" || typeof from === "function") {
      for (let key of __getOwnPropNames(from))
        if (!__hasOwnProp.call(to, key) && key !== except)
          __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
    }
    return to;
  };
  var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

  // qa-linux-wayland-browser-fix/browser-features/modules/modules/pwa/test/SsbCommandLineHandler.test.mts
  var SsbCommandLineHandler_test_exports = {};
  __export(SsbCommandLineHandler_test_exports, {
    runAllTests: () => runAllTests
  });

  // qa-linux-wayland-browser-fix/browser-features/chrome/test/utils/test_harness.ts
  function assert(condition, message) {
    if (!condition) {
      throw new Error(message);
    }
  }
  function assertEquals(actual, expected, message) {
    if (actual !== expected) {
      throw new Error(
        `${message} (expected: ${String(expected)}, actual: ${String(actual)})`
      );
    }
  }
  async function runTests(moduleName, tests) {
    const failures = [];
    for (const [index, test] of tests.entries()) {
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "running",
        index: index + 1,
        total: tests.length,
        startedAtMs: Date.now()
      };
      try {
        await test.fn();
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "passed",
          index: index + 1,
          total: tests.length,
          startedAtMs
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`${test.name}: ${message}`);
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "failed",
          index: index + 1,
          total: tests.length,
          startedAtMs
        };
      }
    }
    globalThis.__NORA_TEST_PROGRESS__ = {
      moduleName,
      testName: "",
      status: "done",
      index: tests.length,
      total: tests.length,
      startedAtMs: Date.now()
    };
    if (failures.length > 0) {
      throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
    }
  }

  // qa-linux-wayland-browser-fix/browser-features/modules/modules/pwa/test/SsbCommandLineHandler.test.mts
  async function testFirstNavigationUsesCachedContainer() {
    const { SsbRunnerUtils } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs"
    );
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs"
    );
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs"
    );
    const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
      "resource://gre/modules/Timer.sys.mjs"
    );
    const id = Services.uuid.generateUUID().toString();
    const url = `http://127.0.0.1:65534/pwa-initial-context-test/${id.replace(/[{}]/g, "")}`;
    const assignmentPref = "floorp.experiments.assignments.v1";
    const hadAssignmentPref = Services.prefs.prefHasUserValue(assignmentPref);
    const oldAssignmentPref = Services.prefs.getStringPref(assignmentPref, "{}");
    const oldAssignments = Experiments.assignments;
    const oldManifestAvailable = Experiments.manifestAvailable;
    const container = ContextualIdentityService.create(
      "PWA Initial Navigation Test",
      "fingerprint",
      "blue"
    );
    let resolveRequest;
    let observerRegistered = false;
    let timer;
    let openedWindow;
    const observer = {
      observe(subject) {
        const channel = subject.QueryInterface(Ci.nsIHttpChannel);
        if (channel.URI.spec !== url) return;
        const requestContext = channel.loadInfo.originAttributes.userContextId ?? 0;
        channel.cancel(Cr.NS_BINDING_ABORTED);
        resolveRequest?.(requestContext);
      }
    };
    try {
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
            assignedAt: (/* @__PURE__ */ new Date()).toISOString()
          }
        })
      );
      assertEquals(
        Experiments.getVariant("pwa_container_support"),
        null,
        "The experiment is not initialized in memory"
      );
      assertEquals(
        Experiments.getCachedEnrollment("pwa_container_support").variantId,
        "enabled",
        "Persisted enrollment is available before initialization"
      );
      const firstRequest = new Promise((resolve) => {
        resolveRequest = resolve;
      });
      Services.obs.addObserver(observer, "http-on-modify-request");
      observerRegistered = true;
      openedWindow = await SsbRunnerUtils.openSsbWindow({
        id,
        name: "Initial Navigation Test",
        start_url: url,
        icon: "",
        userContextId: container.userContextId
      });
      const timeout = new Promise((_resolve, reject) => {
        timer = setTimeout(
          () => reject(new Error("No initial PWA HTTP request was observed")),
          15e3
        );
      });
      const initialContext = await Promise.race([firstRequest, timeout]);
      assertEquals(
        initialContext,
        container.userContextId,
        "The very first HTTP request uses the assigned container"
      );
      assert(
        openedWindow.gBrowser.selectedTab.getAttribute("usercontextid") === String(container.userContextId),
        "The initial tab uses the same container"
      );
    } finally {
      if (timer !== void 0) clearTimeout(timer);
      if (observerRegistered) {
        Services.obs.removeObserver(observer, "http-on-modify-request");
      }
      openedWindow?.close();
      for (const window of Services.wm.getEnumerator("navigator:browser")) {
        try {
          const args = window.arguments;
          if (args?.[1]?.getPropertyAsAString("ssbid") === id) window.close();
        } catch {
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
  async function testLinuxWindowsKeepDistinctLauncherClasses() {
    const { SsbRunnerUtils } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs"
    );
    const { TaskbarExperiment } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs"
    );
    const oldGate = TaskbarExperiment.isEnabledForLinux();
    const apps = ["Alpha", "Beta"].map((name) => ({
      id: Services.uuid.generateUUID().toString(),
      name: `Window Class Test ${name}`,
      start_url: "about:blank",
      icon: ""
    }));
    const windows = [];
    try {
      for (const app of apps) {
        const win = await SsbRunnerUtils.openSsbWindow(app);
        windows.push(win);
        assertEquals(
          win.document.documentElement.getAttribute("windowclass"),
          `floorp-${app.id.slice(1, -1)}`,
          "Each created PWA receives its generated desktop launcher identity"
        );
      }
      assert(
        windows[0].document.documentElement.getAttribute("windowclass") !== windows[1].document.documentElement.getAttribute("windowclass"),
        "Two PWA windows in one process keep different classes"
      );
      assertEquals(
        TaskbarExperiment.isEnabledForLinux(),
        oldGate,
        "Establishing window identity does not change the rollout gate"
      );
    } finally {
      for (const win of windows) win.close();
    }
  }
  async function testLinuxLegacyIdsKeepFallbackWindowClasses() {
    const { SsbRunnerUtils } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs"
    );
    const cases = [
      {
        id: void 0,
        name: "Legacy Missing",
        expected: "floorp-legacy-missing"
      },
      { id: "{}", name: "Legacy Braces", expected: "floorp-legacy-braces" },
      { id: 123, name: "Legacy Number", expected: "floorp-legacy-number" },
      {
        id: { invalid: true },
        name: "Legacy Object",
        expected: "floorp-legacy-object"
      }
    ];
    for (const { id, name, expected } of cases) {
      const app = {
        id,
        name,
        start_url: "about:blank",
        icon: ""
      };
      const win = await SsbRunnerUtils.openSsbWindow(app);
      try {
        assertEquals(
          win.document.documentElement.getAttribute("windowclass"),
          expected,
          "An unusable legacy id falls back to the existing name identity"
        );
      } finally {
        win.close();
      }
    }
  }
  async function testLinuxPwaStartupKeepsOrdinaryBrowserIdentity() {
    const { LinuxSupport } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/supports/Linux.sys.mjs"
    );
    const { SsbRunnerUtils } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs"
    );
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const { AppConstants } = ChromeUtils.importESModule(
      "resource://gre/modules/AppConstants.sys.mjs"
    );
    const category = "browser-window-before-initial-xul-layout-document-preparation";
    const entry = "resource://noraneko/modules/pwa/supports/Linux.sys.mjs";
    let previous;
    try {
      previous = Services.catMan.getCategoryEntry(category, entry);
    } catch {
    }
    const windows = [];
    try {
      LinuxSupport.registerBrowserWindowIdentity();
      const args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      const url = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      url.data = "about:blank";
      args.appendElement(url);
      const browser = await BrowserWindowTracker.promiseOpenWindow({ args });
      windows.push(browser);
      const appInfo = Services.appinfo;
      assertEquals(
        browser.document.documentElement.getAttribute("windowclass"),
        appInfo.remotingName || AppConstants.MOZ_APP_NAME,
        "The pre-layout category gives ordinary windows the browser identity"
      );
      const app = {
        id: Services.uuid.generateUUID().toString(),
        name: "Browser Identity Regression",
        start_url: "about:blank",
        icon: ""
      };
      const pwa = await SsbRunnerUtils.openSsbWindow(app);
      windows.push(pwa);
      assertEquals(
        pwa.document.documentElement.getAttribute("windowclass"),
        LinuxSupport.getWindowClass(app),
        "Browser identity preparation preserves the PWA launcher identity"
      );
    } finally {
      for (const win of windows) win.close();
      if (previous === void 0) {
        Services.catMan.deleteCategoryEntry(category, entry, false);
      } else {
        Services.catMan.addCategoryEntry(category, entry, previous, false, true);
      }
    }
  }
  async function runAllTests() {
    if (Services.appinfo.OS !== "Linux") {
      console.info(
        "[SsbCommandLineHandler.test] Initial Linux container navigation requires Linux; skipped"
      );
      return;
    }
    await runTests("SsbCommandLineHandler.test.mts", [{
      name: "the first Linux PWA request uses its container before experiment initialization",
      fn: testFirstNavigationUsesCachedContainer
    }, {
      name: "Linux PWA windows keep their distinct generated launcher classes",
      fn: testLinuxWindowsKeepDistinctLauncherClasses
    }, {
      name: "Linux PWA legacy ids retain fallback classes without failing startup",
      fn: testLinuxLegacyIdsKeepFallbackWindowClasses
    }, {
      name: "Linux PWA startup keeps ordinary browser and PWA identities separate",
      fn: testLinuxPwaStartupKeepsOrdinaryBrowserIdentity
    }]);
  }
  return __toCommonJS(SsbCommandLineHandler_test_exports);
})();
