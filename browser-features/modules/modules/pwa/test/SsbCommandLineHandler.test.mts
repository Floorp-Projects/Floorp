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
  }]);
}
