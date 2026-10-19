// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Actual frozen packages: no authored stand-in, no package JavaScript edits.
import { CRXConverterClass } from "../../../CRXConverter.sys.mts";
import {
  assert,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until(read, label) {
  const end = Date.now() + 25000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw Error("Timed out waiting for " + label);
}
async function goFullPage() {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow("navigator:browser"),
    id = "fdpohaocaechififmbbbbbknoalclacl";
  const frozen = JSON.parse(
    await IOUtils.readUTF8(
      "/workspace/Floorp/tools/chrome-extension-runtime/corpus-lock.json",
    ),
  ).extensions.find((record) => record.id === id);
  const packageBytes = await IOUtils.read(
    "/workspace/Floorp/_dist/chrome-extension-corpus/packages/" + id + ".crx",
  );
  const digest = [
    ...new Uint8Array(await crypto.subtle.digest("SHA-256", packageBytes)),
  ].map((value) => value.toString(16).padStart(2, "0")).join("");
  assert(digest === frozen.packageSha256, "exact frozen Store package");
  const xpi = PathUtils.join(
    PathUtils.tempDir,
    "floorp-actual-gofullpage-" + Services.uuid.generateUUID() + ".xpi",
  );
  const target = window.gBrowser.addTab(
    window.wrappedJSObject.__floorpRuntimeFixtureURL + "corpus-capture.html",
    {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    },
  );
  window.gBrowser.selectedTab = target;
  const report = {
    id,
    version: frozen.version,
    packageSha256: digest,
    primaryOperationResult: "not-run",
    views: [],
    console: [],
  };
  let addon;
  const consoleListener = {
    observe(message) {
      try {
        const text = message.message;
        if (
          /filesystem|requestFileSystem|gofullpage|fdpohaocaechififmbbbbbknoalclacl/i
            .test(text)
        ) report.console.push(text);
      } catch {}
    },
  };
  Services.console.registerListener(consoleListener);
  try {
    const conversion = await new CRXConverterClass().convert(
      packageBytes.buffer,
      id,
      { id, name: frozen.name, version: frozen.version },
    );
    assert(
      conversion.success && conversion.xpiData,
      "production conversion succeeds: " + conversion.error,
    );
    report.conversionWarnings = conversion.warnings;
    await IOUtils.write(xpi, new Uint8Array(conversion.xpiData));
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(xpi));
    const extension = await until(
      () =>
        ExtensionParent.GlobalManager.extensionMap.get(
          id + "@chromium-extension",
        ),
      "real GoFullPage extension",
    );
    await until(
      () =>
        target.linkedBrowser.contentTitle === "Authored capture corpus page",
      "authored capture fixture",
    );
    const action = await extension.apiManager.asyncGetAPI(
      "browserAction",
      extension,
      "addon_parent",
    );
    await until(
      () => extension.backgroundContext,
      "installed extension background",
    );
    await new Promise((resolve) => setTimeout(resolve, 600));
    window.gBrowser.selectedTab = target;
    report.source = {
      url: target.linkedBrowser.currentURI.spec,
      id: extension.tabManager.getWrapper(target).id,
    };
    action.triggerAction(window);
    await until(
      () =>
        [...extension.views].some((view) =>
          view.viewType === "popup" || view.uri?.spec.includes("/capture.html")
        ),
      "actual extension action popup",
    );
    await new Promise((resolve) => setTimeout(resolve, 4000));
    report.views = [...extension.views].map((view) => ({
      type: view.viewType,
      title: view.xulBrowser?.contentTitle,
      url: view.uri?.spec,
    }));
    const popup = [...extension.views].find((view) =>
      view.viewType === "popup"
    );
    if (popup) {
      const tabsAPI = await extension.apiManager.asyncGetAPI(
        "tabs",
        extension,
        "addon_parent",
      );
      report.activeTabs = await tabsAPI.getAPI(popup).tabs.query({
        active: true,
        currentWindow: true,
      });
      report.activeTabGrant = extension.tabManager.hasActiveTabPermission(
        target,
      );
      report.popup = await new Promise((resolve) => {
        const manager = popup.xulBrowser.messageManager;
        function listener(message) {
          manager.removeMessageListener(
            "Floorp:CaptureCorpusSnapshot",
            listener,
          );
          resolve(message.data);
        }
        manager.addMessageListener("Floorp:CaptureCorpusSnapshot", listener);
        manager.loadFrameScript(
          "data:application/javascript," +
            encodeURIComponent(
              'sendAsyncMessage("Floorp:CaptureCorpusSnapshot",{text:content.document.body.innerText,requestFileSystem:typeof content.wrappedJSObject.webkitRequestFileSystem,persistentStorage:typeof content.wrappedJSObject.navigator.webkitPersistentStorage,location:content.location.href});',
            ),
          false,
        );
      });
    }
    const result = [...extension.views].find((view) =>
      view.uri?.spec.includes("/capture.html")
    );
    if (!result) {
      report.primaryOperationResult = "failure";
      throw Error(
        "Actual frozen GoFullPage did not create its saved capture view; see /tmp/floorp-capture-corpus-native.json",
      );
    }
    report.primaryOperationResult = "needs-image-verification";
  } finally {
    Services.console.unregisterListener(consoleListener);
    await IOUtils.writeUTF8(
      "/tmp/floorp-capture-corpus-native.json",
      JSON.stringify(report, null, 2),
    );
    await addon?.uninstall();
    if (target.isConnected) window.gBrowser.removeTab(target);
    await IOUtils.remove(xpi, { ignoreAbsent: true });
  }
}
export async function runAllTests() {
  await runTests("CaptureCorpusNative.test.mts", [{
    name: "actual frozen GoFullPage captures authored page",
    fn: goFullPage,
  }]);
}
