// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Actual frozen packages: no authored stand-in, no package JavaScript edits.
import { installFrozenCorpus } from "../../../../../../../tools/chrome-extension-runtime/corpus-install-fixture.mjs";
import { invokeNativeCorpusAction } from "../../../../../../../tools/chrome-extension-runtime/corpus-native-action.mjs";
import type {
  NativeCorpusTab,
  NativeCorpusWindow,
} from "../../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
import type {
  CaptureCorpusAccountEvidence,
  CaptureCorpusAddon,
  CaptureCorpusBrowser,
  CaptureCorpusCanvas,
  CaptureCorpusDownloadsModule,
  CaptureCorpusExtension,
  CaptureCorpusExtensionParentModule,
  CaptureCorpusFrozenLock,
  CaptureCorpusImage,
  CaptureCorpusInspectionSource,
  CaptureCorpusPermissionPanel,
  CaptureCorpusReply,
  CaptureCorpusReport,
  CaptureCorpusTruthy,
  CaptureCorpusWindow,
} from "../../helpers/capture-corpus-test-types.ts";
import {
  assert,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until<Value>(
  read: () => Value | Promise<Value>,
  label: string,
): Promise<CaptureCorpusTruthy<Value>> {
  const end = Date.now() + 25000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value as CaptureCorpusTruthy<Value>;
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw Error("Timed out waiting for " + label);
}
function inspect<Result>(
  browser: CaptureCorpusBrowser,
  source: CaptureCorpusInspectionSource<Result>,
): Promise<Result> {
  const name = "Floorp:CaptureCorpus:" + Services.uuid.generateUUID();
  return new Promise<Result>((resolve, reject) => {
    const manager = browser.messageManager;
    const timer = setTimeout(() => {
      try {
        manager.removeMessageListener(name, listener);
      } catch {
        // A closed extension view can already have destroyed its manager.
      }
      reject(Error("Extension view closed before its snapshot reply"));
    }, 6000);
    const listener = (message: CaptureCorpusReply<Result>) => {
      clearTimeout(timer);
      manager.removeMessageListener(name, listener);
      message.data.error
        ? reject(Error(message.data.error))
        : resolve(message.data.value as Result);
    };
    manager.addMessageListener(name, listener);
    manager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `(async()=>{try{const value=await (${source})();sendAsyncMessage(${
            JSON.stringify(
              name,
            )
          },{value});}catch(error){sendAsyncMessage(${
            JSON.stringify(
              name,
            )
          },{error:String(error)+error.stack});}})();`,
        ),
      false,
    );
  });
}
function trustedClick(
  browser: CaptureCorpusBrowser,
  selector: string,
  text = false,
) {
  return until(
    () =>
      inspect<boolean | null>(
        browser,
        `()=>{
    const e=${
          text
            ? `[...content.document.querySelectorAll('button,a,[role="button"],span,div')].filter(e=>e.textContent.trim()===${
              JSON.stringify(
                selector,
              )
            }&&e.getBoundingClientRect().width).sort((a,b)=>a.children.length-b.children.length)[0]`
            : `content.document.querySelector(${JSON.stringify(selector)})`
        };
    if(!e)return null;e.scrollIntoView({block:'center',inline:'center'});const r=e.getBoundingClientRect();if(!r.width||!r.height)return null;const hit=content.document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);if(!(hit===e||e.contains(hit)||e.parentElement===hit))return null;
    content.synthesizeMouseEvent('mousedown',r.x+r.width/2,r.y+r.height/2,{button:0,buttons:1,clickCount:1});content.synthesizeMouseEvent('mouseup',r.x+r.width/2,r.y+r.height/2,{button:0,buttons:0,clickCount:1});return true;
  }`,
      ),
    "actual unobstructed native UI " + selector,
  );
}
async function capturePackage(id: string) {
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as CaptureCorpusExtensionParentModule;
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as CaptureCorpusWindow;
  const initialWindows = new Set(
    Services.wm.getEnumerator("navigator:browser") as unknown as Iterable<
      CaptureCorpusWindow
    >,
  );
  const initialTabs = new Map(
    [...initialWindows].map((
      candidate,
    ) => [candidate, new Set(candidate.gBrowser.tabs)]),
  );
  const fullPage = id === "fdpohaocaechififmbbbbbknoalclacl";
  const accountGate = [
    "liecbddmkiiihnedobmlmillhodjkdmb",
    "mmeijimgabbpbgpdklnllpncmdofkcpn",
  ].includes(id);
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  assert(root, "runner-provided frozen corpus root is required");
  const frozen = (JSON.parse(
    await IOUtils.readUTF8(
      PathUtils.join(
        root,
        "tools",
        "chrome-extension-runtime",
        "corpus-lock.json",
      ),
    ),
  ) as CaptureCorpusFrozenLock).extensions.find((record) => record.id === id);
  assert(
    frozen?.version && frozen.packageSha256,
    "exact frozen package metadata is present",
  );
  const packageBytes = await IOUtils.read(
    PathUtils.join(
      root,
      "_dist",
      "chrome-extension-corpus",
      "packages",
      id + ".crx",
    ),
  ) as Uint8Array<ArrayBuffer>;
  const digest = [
    ...new Uint8Array(await crypto.subtle.digest("SHA-256", packageBytes)),
  ]
    .map((value) => value.toString(16).padStart(2, "0"))
    .join("");
  assert(digest === frozen.packageSha256, "exact frozen Store package");
  const target = window.gBrowser.addTab(
    window.wrappedJSObject.__floorpRuntimeFixtureURL + "corpus-capture.html",
    {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    },
  );
  window.gBrowser.selectedTab = target;
  const report: CaptureCorpusReport = {
    stage: "actual-package-primary-operation",
    nativeQualified: false,
    cleanupCompleted: false,
    id,
    version: frozen.version,
    packageSha256: digest,
    primaryOperationResult: "not-run",
    runtimeVersion: Services.appinfo.version,
    runtimeBuildId: Services.appinfo.appBuildID,
    views: [],
    console: [],
  };
  let addon: CaptureCorpusAddon | undefined;
  const output = PathUtils.join(
    Services.env.get("FLOORP_CORPUS_OUTPUT") ||
      PathUtils.join(root, "_dist", "chrome-runtime-tests"),
    "corpus-capture-floorp-" + Services.uuid.generateUUID(),
  );
  report.output = output;
  const xpi = PathUtils.join(output, id + ".xpi");
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
    observe(message: nsIConsoleMessage) {
      try {
        const text = message.message;
        if (
          /filesystem|requestFileSystem|gofullpage|fdpohaocaechififmbbbbbknoalclacl|download|Chrome.*Error/i
            .test(
              text,
            )
        ) {
          report.console.push(text);
        }
      } catch {
        // Not every console notification exposes a readable message string.
      }
    },
  };
  Services.console.registerListener(consoleListener);
  try {
    const installed = await installFrozenCorpus(root, frozen, output);
    addon = installed.addon;
    report.conversionWarnings = installed.warnings;
    report.conversion = installed.conversion;
    report.sourceIntegrity = installed.sourceIntegrity;
    report.nativeEvidence = installed.nativeEvidence;
    const extension = installed.extension as unknown as CaptureCorpusExtension;
    await until(
      () =>
        target.linkedBrowser.contentTitle === "Authored capture corpus page",
      "authored capture fixture",
    );
    await until(
      () => extension.backgroundContext,
      "installed extension background",
    );
    // Real packages open onboarding tabs from their asynchronous onInstalled
    // handlers. Let that UI settle before activating the authored capture tab.
    let tabsSignature = "",
      tabsChanged = Date.now();
    const installedAt = Date.now();
    await until(() => {
      const signature = [
        ...Services.wm.getEnumerator(
          "navigator:browser",
        ) as unknown as Iterable<CaptureCorpusWindow>,
      ]
        .flatMap((candidate) =>
          [...(candidate.gBrowser?.tabs || [])].map(
            (tab) => tab.linkedBrowser.currentURI.spec,
          )
        )
        .join("\n");
      if (signature !== tabsSignature) {
        tabsSignature = signature;
        tabsChanged = Date.now();
      }
      return (
        Date.now() - installedAt >= 2000 && Date.now() - tabsChanged >= 1200
      );
    }, "installed package onboarding tabs settled");
    window.gBrowser.selectedTab = target;
    report.source = {
      url: target.linkedBrowser.currentURI.spec,
      id: extension.tabManager.getWrapper(target).id,
    };
    report.nativeAction = await invokeNativeCorpusAction(
      installed.extension,
      window as unknown as NativeCorpusWindow,
      target as unknown as NativeCorpusTab,
    );
    if (accountGate) {
      const evidence = await until(async () => {
        for (
          const candidate of Services.wm.getEnumerator(
            "navigator:browser",
          ) as unknown as Iterable<CaptureCorpusWindow>
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
            ) {
              return { url, title: tab.linkedBrowser.contentTitle };
            }
          }
        }
        for (const view of extension.views) {
          if (
            (view.viewType === "popup" || view.viewType === "tab") &&
            view.xulBrowser
          ) {
            const snapshot = await inspect<CaptureCorpusAccountEvidence>(
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
            ) {
              return snapshot;
            }
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
        [...extension.views].some(
          (view) =>
            view.viewType === "popup" ||
            view.uri?.spec.includes("/capture.html"),
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
      await trustedClick(popup.xulBrowser!, '[data-tab="screenshot"]');
      await trustedClick(popup.xulBrowser!, "#visible");
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
      report.image = await until(
        () =>
          inspect<CaptureCorpusImage | null>(
            result.xulBrowser!,
            `()=>{
      const image=[...content.document.images].find(image=>image.naturalHeight>2000);
      if(!image)return null;
      const canvas=content.document.createElement('canvas');canvas.width=image.naturalWidth;canvas.height=image.naturalHeight;canvas.getContext('2d').drawImage(image,0,0);
      return {width:image.naturalWidth,height:image.naturalHeight,src:image.src,pixel:Array.from(canvas.getContext('2d').getImageData(20,20,1,1).data)};
    }`,
          ),
        "actual stitched capture PNG decode",
      );
      assert(
        report.image.height === 2590,
        "full authored page stitched to exact2590px height",
      );
    } else {
      report.canvas = await until(
        () =>
          inspect<CaptureCorpusCanvas | false>(
            result.xulBrowser!,
            `()=>{const c=[...content.document.querySelectorAll('canvas')].sort((a,b)=>b.width*b.height-a.width*a.height)[0];return c?.width>500&&{width:c.width,height:c.height,pixel:Array.from(c.getContext('2d').getImageData(20,20,1,1).data)};}`,
          ),
        "actual authored-page screenshot canvas",
      );
      await trustedClick(result.xulBrowser!, "Done", true);
      await until(
        () =>
          inspect(
            result.xulBrowser!,
            () => content.document.body.innerText.includes("Download"),
          ),
        "actual capture download preview",
      );
      if (
        await inspect(
          result.xulBrowser!,
          () => content.document.body.innerText.includes("Got It"),
        )
      ) {
        await trustedClick(result.xulBrowser!, "Got It", true);
      }
      report.image = await until(
        () =>
          inspect<CaptureCorpusImage | null>(
            result.xulBrowser!,
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
    ) as unknown as CaptureCorpusDownloadsModule;
    const downloads = await Downloads.getList(Downloads.ALL);
    await trustedClick(
      result.xulBrowser!,
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
        ) as CaptureCorpusPermissionPanel | null;
        if (panel?.button) {
          report.downloadPermissionPrompt = panel.textContent;
          panel.button.click();
        }
      }
      const entries = await downloads.getAll();
      const saved = entries.find(
        (entry) => PathUtils.parent(entry.target.path) === downloadDir,
      );
      if (saved?.error) {
        throw Error("Actual image download failed: " + saved.error.message);
      }
      return saved?.succeeded && saved;
    }, "real extension Download image UI saved PNG");
    const bytes = await IOUtils.read(download.target.path) as Uint8Array<
      ArrayBuffer
    >;
    assert(
      download.target.path.endsWith(".png") &&
        bytes.length > 10000 &&
        bytes[0] === 137 &&
        bytes[1] === 80 &&
        bytes[2] === 78 &&
        bytes[3] === 71,
      "actual saved PNG file bytes",
    );
    report.media = {
      ...report.image,
      path: download.target.path,
      bytes: bytes.length,
      sha256: [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))]
        .map((value) => value.toString(16).padStart(2, "0"))
        .join(""),
    };
    if (!fullPage) {
      assert(
        report.media.sha256 === report.image.sha256,
        "actual saved PNG exactly matches decoded authored-page preview",
      );
      report.media.src = report.media.src.slice(0, 150);
      report.image.src = report.image.src.slice(0, 150);
    }
    // Decode the actual saved bytes through Gecko, independently of the editor's
    // preview or its filesystem URL. A correct preview cannot qualify a broken export.
    const savedImage = window.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "img",
    ) as HTMLImageElement;
    savedImage.src =
      Services.io.newFileURI(new FileUtils.File(download.target.path)).spec;
    await until(() => {
      if (savedImage.complete && !savedImage.naturalWidth) {
        throw Error("Saved PNG failed native image decoding");
      }
      return savedImage.complete && savedImage.naturalWidth > 0;
    }, "native image decoding of the actual saved PNG");
    const savedCanvas = window.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas",
    ) as HTMLCanvasElement;
    savedCanvas.width = savedImage.naturalWidth;
    savedCanvas.height = savedImage.naturalHeight;
    const savedContext = savedCanvas.getContext("2d");
    assert(savedContext, "native saved-PNG canvas context is available");
    savedContext.drawImage(savedImage, 0, 0);
    report.savedImage = {
      width: savedImage.naturalWidth,
      height: savedImage.naturalHeight,
      pixel: Array.from(savedContext.getImageData(20, 20, 1, 1).data),
    };
    assert(
      report.savedImage.width === report.image.width &&
        report.savedImage.height === report.image.height,
      "actual saved PNG decodes to the complete preview dimensions",
    );
    assert(
      JSON.stringify(report.savedImage.pixel) ===
        JSON.stringify([41, 126, 216, 255]),
      "actual saved PNG independently decodes the authored header pixel",
    );
    report.primaryOperationResult = "pass";
  } catch (error) {
    report.error = String(error);
    if (report.primaryOperationResult === "not-run") {
      report.primaryOperationResult = "failure";
    }
    report.browserPages = [
      ...Services.wm.getEnumerator("navigator:browser") as unknown as Iterable<
        CaptureCorpusWindow
      >,
    ].flatMap((win) =>
      [...(win.gBrowser?.tabs || [])].map((tab) => ({
        url: tab.linkedBrowser.currentURI.spec,
        title: tab.linkedBrowser.contentTitle,
      }))
    );
    const { Downloads } = ChromeUtils.importESModule(
      "resource://gre/modules/Downloads.sys.mjs",
    ) as unknown as CaptureCorpusDownloadsModule;
    report.downloads = (
      await (await Downloads.getList(Downloads.ALL)).getAll()
    ).map((download) => ({
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
        [...extension.views]
          .filter((view) => view.xulBrowser)
          .map(async (view) => ({
            type: view.viewType,
            url: view.uri?.spec,
            snapshot: await inspect(view.xulBrowser!, () => ({
              text: content.document.body?.innerText,
              ready: content.document.readyState,
              location: content.location.href,
            })).catch((error) => String(error)),
          })),
      );
    }
    throw error;
  } finally {
    const cleanupErrors: string[] = [];
    const cleanup = async (operation: () => void | Promise<void>) => {
      try {
        await operation();
      } catch (error) {
        cleanupErrors.push(String(error));
      }
    };
    await cleanup(() => Services.console.unregisterListener(consoleListener));
    for (const pref of previousPrefs) {
      await cleanup(() => {
        if (!pref.user) Services.prefs.clearUserPref(pref.name);
        else if (pref.type === Services.prefs.PREF_STRING) {
          Services.prefs.setStringPref(pref.name, pref.value as string);
        } else if (pref.type === Services.prefs.PREF_INT) {
          Services.prefs.setIntPref(pref.name, pref.value as number);
        } else Services.prefs.setBoolPref(pref.name, pref.value as boolean);
      });
    }
    await cleanup(async () => {
      await addon?.uninstall();
    });
    for (
      const candidate of Services.wm.getEnumerator(
        "navigator:browser",
      ) as unknown as Iterable<CaptureCorpusWindow>
    ) {
      if (!initialWindows.has(candidate)) {
        await cleanup(() => candidate.close());
      } else {
        for (const tab of [...candidate.gBrowser.tabs]) {
          if (!initialTabs.get(candidate)?.has(tab)) {
            await cleanup(() => candidate.gBrowser.removeTab(tab));
          }
        }
      }
    }
    if (target.isConnected) {
      await cleanup(() => window.gBrowser.removeTab(target));
    }
    await cleanup(() => IOUtils.remove(xpi, { ignoreAbsent: true }));
    report.cleanupCompleted = cleanupErrors.length === 0;
    if (cleanupErrors.length) report.cleanupError = cleanupErrors.join("\n");
    report.nativeQualified = report.primaryOperationResult === "pass" &&
      report.cleanupCompleted &&
      report.nativeEvidence?.candidateIdentityVerified === true &&
      report.nativeEvidence.nativeBackgroundVerified === true;
    await IOUtils.writeUTF8(
      PathUtils.join(output, "report.json"),
      JSON.stringify(report, null, 2),
    );
    await IOUtils.writeUTF8(
      PathUtils.join(PathUtils.tempDir, "floorp-capture-corpus-native.json"),
      JSON.stringify(report, null, 2),
    );
    assert(
      !cleanupErrors.length,
      "actual capture cleanup completes: " + cleanupErrors.join("; "),
    );
  }
}
export async function runAllTests() {
  const only = Services.env
    .get("FLOORP_CAPTURE_CORPUS_IDS")
    .split(",")
    .filter(Boolean);
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
  const selected = records.filter(
    (record) => !only.length || only.includes(record.id),
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
