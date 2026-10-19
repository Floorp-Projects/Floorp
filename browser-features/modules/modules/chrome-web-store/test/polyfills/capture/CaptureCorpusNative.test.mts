// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Actual frozen packages: no authored stand-in, no package JavaScript edits.
import { CRXConverterClass } from "../../../CRXConverter.sys.mts";
import {
  assert,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
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
async function inspect(browser, source) {
  const name = "Floorp:CaptureCorpus:" + Services.uuid.generateUUID();
  return new Promise((resolve, reject) => {
    const manager = browser.messageManager;
    const timer = setTimeout(() => {
      try {
        manager.removeMessageListener(name, listener);
      } catch {}
      reject(Error("Extension view closed before its snapshot reply"));
    }, 6000);
    const listener = (message) => {
      clearTimeout(timer);
      manager.removeMessageListener(name, listener);
      message.data.error
        ? reject(Error(message.data.error))
        : resolve(message.data.value);
    };
    manager.addMessageListener(name, listener);
    manager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `(async()=>{try{const value=await (${source})();sendAsyncMessage(${
            JSON.stringify(name)
          },{value});}catch(error){sendAsyncMessage(${
            JSON.stringify(name)
          },{error:String(error)+error.stack});}})();`,
        ),
      false,
    );
  });
}
async function trustedClick(browser, selector, text = false) {
  return until(() =>
    inspect(
      browser,
      `()=>{
    const e=${
        text
          ? `[...content.document.querySelectorAll('button,a,[role="button"],span,div')].filter(e=>e.textContent.trim()===${
            JSON.stringify(selector)
          }&&e.getBoundingClientRect().width).sort((a,b)=>a.children.length-b.children.length)[0]`
          : `content.document.querySelector(${JSON.stringify(selector)})`
      };
    if(!e)return null;e.scrollIntoView({block:'center',inline:'center'});const r=e.getBoundingClientRect();if(!r.width||!r.height)return null;const hit=content.document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);if(!(hit===e||e.contains(hit)||e.parentElement===hit))return null;
    content.synthesizeMouseEvent('mousedown',r.x+r.width/2,r.y+r.height/2,{button:0,buttons:1,clickCount:1});content.synthesizeMouseEvent('mouseup',r.x+r.width/2,r.y+r.height/2,{button:0,buttons:0,clickCount:1});return true;
  }`,
    ), "actual unobstructed native UI " + selector);
}
async function capturePackage(id) {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const initialWindows = new Set(
    Services.wm.getEnumerator("navigator:browser"),
  );
  const fullPage = id === "fdpohaocaechififmbbbbbknoalclacl";
  const accountGate = [
    "liecbddmkiiihnedobmlmillhodjkdmb",
    "mmeijimgabbpbgpdklnllpncmdofkcpn",
  ].includes(id);
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
    runtimeVersion: Services.appinfo.version,
    runtimeBuildId: Services.appinfo.appBuildID,
    views: [],
    console: [],
  };
  let addon;
  const output = PathUtils.join(
    "/workspace/Floorp/_dist/chrome-runtime-tests",
    "corpus-capture-floorp-" + Services.uuid.generateUUID(),
  );
  const downloadDir = PathUtils.join(output, "downloads");
  await IOUtils.makeDirectory(downloadDir, { createAncestors: true });
  const prefNames = [
    "browser.download.dir",
    "browser.download.folderList",
    "browser.download.useDownloadDir",
  ];
  const previousPrefs = prefNames.map((name) => ({
    name,
    type: Services.prefs.getPrefType(name),
    user: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getPrefType(name) === Services.prefs.PREF_STRING
      ? Services.prefs.getStringPref(name)
      : Services.prefs.getPrefType(name) === Services.prefs.PREF_INT
      ? Services.prefs.getIntPref(name)
      : Services.prefs.getBoolPref(name, false),
  }));
  Services.prefs.setStringPref("browser.download.dir", downloadDir);
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setBoolPref("browser.download.useDownloadDir", true);
  const consoleListener = {
    observe(message) {
      try {
        const text = message.message;
        if (
          /filesystem|requestFileSystem|gofullpage|fdpohaocaechififmbbbbbknoalclacl|download|Chrome.*Error/i
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
    await action.triggerAction(window);
    if (accountGate) {
      const evidence = await until(async () => {
        for (
          const candidate of Services.wm.getEnumerator("navigator:browser")
        ) {
          for (const tab of candidate.gBrowser?.tabs || []) {
            const url = tab.linkedBrowser.currentURI.spec;
            if (
              id === "liecbddmkiiihnedobmlmillhodjkdmb" &&
              ((url.startsWith("https://id.atlassian.com/") &&
                url.includes("loom")) ||
                /^https:\/\/(?:[^/]+\.)?loom\.com\/(?:.*login|.*signup)/.test(
                  url,
                ))
            ) return { url, title: tab.linkedBrowser.contentTitle };
          }
        }
        for (const view of extension.views) {
          if (
            (view.viewType === "popup" || view.viewType === "tab") &&
            view.xulBrowser
          ) {
            const snapshot = await inspect(
              view.xulBrowser,
              () => ({
                text: content.document.body?.innerText,
                title: content.document.title,
                url: content.location.href,
              }),
            ).catch(() => null);
            if (
              snapshot &&
              /sign in|log in|create an account|login|sign up/i.test(
                snapshot.text + " " + snapshot.title,
              )
            ) return snapshot;
          }
        }
        return false;
      }, "authentic extension account prerequisite");
      report.accountEvidence = evidence;
      report.primaryOperationResult = "blocked-account";
      return;
    }

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
    if (!fullPage) {
      const popup = await until(
        () => [...extension.views].find((view) => view.viewType === "popup"),
        "actual screenshot popup",
      );
      await trustedClick(popup.xulBrowser, '[data-tab="screenshot"]');
      await trustedClick(popup.xulBrowser, "#visible");
    }
    const result = await until(
      () =>
        [...extension.views].find((view) =>
          view.uri?.spec.includes(
            fullPage ? "/capture.html" : "/edit-react.html",
          )
        ),
      "actual frozen extension completed capture",
    );
    if (fullPage) {
      report.image = await until(async () =>
        inspect(
          result.xulBrowser,
          `()=>{
      const image=[...content.document.images].find(image=>image.naturalHeight>2000);
      if(!image)return null;
      const canvas=content.document.createElement('canvas');canvas.width=image.naturalWidth;canvas.height=image.naturalHeight;canvas.getContext('2d').drawImage(image,0,0);
      return {width:image.naturalWidth,height:image.naturalHeight,src:image.src,pixel:Array.from(canvas.getContext('2d').getImageData(20,20,1,1).data)};
    }`,
        ), "actual stitched capture PNG decode");
      assert(
        report.image.height === 2590,
        "full authored page stitched to exact2590px height",
      );
    } else {
      report.canvas = await until(
        () =>
          inspect(
            result.xulBrowser,
            `()=>{const c=[...content.document.querySelectorAll('canvas')].sort((a,b)=>b.width*b.height-a.width*a.height)[0];return c?.width>500&&{width:c.width,height:c.height,pixel:Array.from(c.getContext('2d').getImageData(20,20,1,1).data)};}`,
          ),
        "actual authored-page screenshot canvas",
      );
      await trustedClick(result.xulBrowser, "Done", true);
      await until(
        () =>
          inspect(
            result.xulBrowser,
            () => content.document.body.innerText.includes("Download"),
          ),
        "actual capture download preview",
      );
      if (
        await inspect(
          result.xulBrowser,
          () => content.document.body.innerText.includes("Got It"),
        )
      ) await trustedClick(result.xulBrowser, "Got It", true);
      report.image = await until(
        () =>
          inspect(
            result.xulBrowser,
            `async()=>{const image=[...content.document.images].find(image=>image.naturalWidth>500);if(!image)return null;const canvas=content.document.createElement('canvas');canvas.width=image.naturalWidth;canvas.height=image.naturalHeight;canvas.getContext('2d').drawImage(image,0,0);const encoded=content.atob(image.src.split(',')[1]);const bytes=Uint8Array.from(encoded,letter=>letter.charCodeAt(0));const sha256=Array.from(new Uint8Array(Cu.cloneInto(await content.crypto.subtle.digest('SHA-256',bytes),globalThis))).map(value=>value.toString(16).padStart(2,'0')).join('');return {width:image.naturalWidth,height:image.naturalHeight,src:image.src,sha256,pixel:Array.from(canvas.getContext('2d').getImageData(20,20,1,1).data)};}`,
          ),
        "actual download preview image",
      );
    }
    assert(
      JSON.stringify(report.image.pixel) ===
        JSON.stringify([41, 126, 216, 255]),
      "captured authored header pixel",
    );
    const { Downloads } = ChromeUtils.importESModule(
      "resource://gre/modules/Downloads.sys.mjs",
    );
    const downloads = await Downloads.getList(Downloads.ALL);
    await trustedClick(
      result.xulBrowser,
      fullPage ? "#btn-download" : "Download",
      !fullPage,
    );
    const download = await until(async () => {
      const notification = window.PopupNotifications.getNotification(
        "addon-webext-permissions",
      );
      if (notification && window.PopupNotifications.panel.state === "open") {
        const panel = window.PopupNotifications.panel.querySelector(
          "popupnotification",
        );
        if (panel?.button) {
          report.downloadPermissionPrompt = panel.textContent;
          panel.button.click();
        }
      }
      const entries = await downloads.getAll();
      const saved = entries.find((entry) =>
        PathUtils.parent(entry.target.path) === downloadDir
      );
      if (saved?.error) {
        throw Error("Actual image download failed: " + saved.error.message);
      }
      return saved?.succeeded && saved;
    }, "real extension Download image UI saved PNG");
    const bytes = await IOUtils.read(download.target.path);
    assert(
      download.target.path.endsWith(".png") && bytes.length > 10000 &&
        bytes[0] === 137 && bytes[1] === 80 &&
        bytes[2] === 78 && bytes[3] === 71,
      "actual saved PNG file bytes",
    );
    report.media = {
      ...report.image,
      path: download.target.path,
      bytes: bytes.length,
      sha256: [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))]
        .map((value) => value.toString(16).padStart(2, "0")).join(""),
    };
    if (!fullPage) {
      assert(
        report.media.sha256 === report.image.sha256,
        "actual saved PNG exactly matches decoded authored-page preview",
      );
      report.media.src = report.media.src.slice(0, 150);
      report.image.src = report.image.src.slice(0, 150);
    }
    report.primaryOperationResult = "pass";
  } catch (error) {
    report.error = String(error);
    if (report.primaryOperationResult === "not-run") {
      report.primaryOperationResult = "failure";
    }
    report.browserPages = [...Services.wm.getEnumerator("navigator:browser")]
      .flatMap((win) =>
        [...(win.gBrowser?.tabs || [])].map((tab) => ({
          url: tab.linkedBrowser.currentURI.spec,
          title: tab.linkedBrowser.contentTitle,
        }))
      );
    const { Downloads } = ChromeUtils.importESModule(
      "resource://gre/modules/Downloads.sys.mjs",
    );
    report.downloads = (await (await Downloads.getList(Downloads.ALL)).getAll())
      .map((download) => ({
        url: download.source.url,
        path: download.target.path,
        succeeded: download.succeeded,
        error: download.error?.message,
      }));
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      id + "@chromium-extension",
    );
    if (extension) {
      report.finalViews = await Promise.all(
        [...extension.views].filter((view) => view.xulBrowser).map(
          async (view) => ({
            type: view.viewType,
            url: view.uri?.spec,
            snapshot: await inspect(
              view.xulBrowser,
              () => ({
                text: content.document.body?.innerText,
                ready: content.document.readyState,
                location: content.location.href,
              }),
            ).catch((error) => String(error)),
          }),
        ),
      );
    }
    throw error;
  } finally {
    Services.console.unregisterListener(consoleListener);
    await IOUtils.writeUTF8(
      "/tmp/floorp-capture-corpus-native.json",
      JSON.stringify(report, null, 2),
    );
    for (const pref of previousPrefs) {
      if (!pref.user) Services.prefs.clearUserPref(pref.name);
      else if (pref.type === Services.prefs.PREF_STRING) {
        Services.prefs.setStringPref(pref.name, pref.value);
      } else if (pref.type === Services.prefs.PREF_INT) {
        Services.prefs.setIntPref(pref.name, pref.value);
      } else Services.prefs.setBoolPref(pref.name, pref.value);
    }
    await IOUtils.writeUTF8(
      PathUtils.join(output, "report.json"),
      JSON.stringify(report, null, 2),
    );
    await addon?.uninstall();
    for (const candidate of Services.wm.getEnumerator("navigator:browser")) {
      if (!initialWindows.has(candidate)) candidate.close();
    }
    if (target.isConnected) window.gBrowser.removeTab(target);
    await IOUtils.remove(xpi, { ignoreAbsent: true });
  }
}
export async function runAllTests() {
  const only = Services.env.get("FLOORP_CAPTURE_CORPUS_IDS").split(",").filter(
    Boolean,
  );
  const records = [
    {
      id: "fdpohaocaechififmbbbbbknoalclacl",
      name: "actual frozen GoFullPage captures authored page",
    },
    {
      id: "nlipoenfbbikpbjkfpfillcgkoblgpmj",
      name: "actual frozen Awesome Screenshot captures authored page",
    },
    {
      id: "liecbddmkiiihnedobmlmillhodjkdmb",
      name: "actual frozen Loom account prerequisite",
    },
    {
      id: "mmeijimgabbpbgpdklnllpncmdofkcpn",
      name: "actual frozen Screencastify account prerequisite",
    },
  ];
  assert(
    only.every((id) => records.some((record) => record.id === id)),
    "every requested frozen package has an implemented native recipe",
  );
  const selected = records.filter((record) =>
    !only.length || only.includes(record.id)
  );
  assert(selected.length > 0, "actual package recipes must execute");
  await runTests(
    "CaptureCorpusNative.test.mts",
    selected.map((record) => ({
      name: record.name,
      fn: () => capturePackage(record.id),
    })),
  );
}
