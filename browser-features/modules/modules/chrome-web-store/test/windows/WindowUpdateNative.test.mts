// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type {
  WindowUpdateAddon,
  WindowUpdateExtension,
  WindowUpdateNativeEvent,
  WindowUpdateNativeWindow,
  WindowUpdateReport,
} from "./window-update-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const sleep = (milliseconds: number) =>
  new Promise<void>((resolve) => setTimeout(resolve, milliseconds));

async function waitFor<T>(
  read: () => T | undefined,
  label: string,
): Promise<T> {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = read();
    if (value !== undefined) return value;
    await sleep(25);
  }
  throw new Error(`Timeout: ${label}`);
}

async function testWindowUpdate(marked: boolean): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const originalId = "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh";
  const addonId = marked
    ? `${originalId}@chromium-extension`
    : "window-update-firefox@floorp.test";
  const background = `
    const marked=${marked};
    let resume;
    const pause=value=>new Promise(resolve=>{resume=resolve;document.title=JSON.stringify(value);});
    const check=(value,message)=>{if(!value)throw Error(message);};
    const update=(id,options)=>new Promise((resolve,reject)=>chrome.windows.update(id,options,value=>{
      if(chrome.runtime.lastError)reject(Error(chrome.runtime.lastError.message));else resolve(value);
    }));
    async function run(){
      const created=await chrome.windows.create({type:'normal',state:'normal',width:900,height:700});
      const id=created.id;
      const before=await chrome.windows.get(id);
      await pause({phase:'ready',targetId:id,before});
      const requested={width:before.width-80,height:before.height-60};
      const started=Date.now();
      const returned=await update(id,requested);
      const immediate=await chrome.windows.get(id);
      await pause({phase:'normal',targetId:id,before,requested,returned,immediate,elapsed:Date.now()-started});
      if(!marked){await chrome.windows.remove(id);document.title=JSON.stringify({phase:'done'});return;}
      const first=chrome.windows.update(id,{left:100});
      const second=chrome.windows.update(id,{top:200});
      const queuedFirst=await first;
      const queuedSecond=await second;
      check(queuedSecond.left===queuedFirst.left,'Queued top-only update reset the acknowledged left coordinate');
      await pause({phase:'queued',targetId:id,queuedFirst,returned:queuedSecond,immediate:await chrome.windows.get(id)});
      const clampStarted=Date.now();
      const clamped=await update(id,{width:1,height:1});
      await pause({phase:'clamped',targetId:id,requested:{width:1,height:1},returned:clamped,immediate:await chrome.windows.get(id),elapsed:Date.now()-clampStarted});
      await chrome.windows.update(id,{state:'maximized'});
      const maximized=await chrome.windows.get(id);
      const ignoredRequest={width:maximized.width-80,height:maximized.height-60};
      const ignoredStarted=Date.now();
      const ignored=await update(id,ignoredRequest);
      await pause({phase:'possibly-ignored',targetId:id,before:maximized,requested:ignoredRequest,returned:ignored,immediate:await chrome.windows.get(id),elapsed:Date.now()-ignoredStarted});
      const closingWindow=await chrome.windows.create({type:'normal',state:'normal',width:900,height:700});
      const closingBefore=await chrome.windows.get(closingWindow.id);
      const closing=update(closingWindow.id,{width:1,height:1}).then(value=>({returned:value}),error=>({error:String(error)}));
      await pause({phase:'window-close-ready',targetId:closingWindow.id,before:closingBefore});
      await chrome.windows.remove(closingWindow.id);
      const closed=await closing;
      check(typeof closed.error==='string'&&/closed/i.test(closed.error),'Closing the target did not cancel its pending native acknowledgement');
      await pause({phase:'window-closed',targetId:id,error:closed.error});
      await chrome.windows.update(id,{state:'normal',width:900,height:700});
      const contextOriginal=await chrome.windows.get(id);
      // The first nonmatching minimum-size request remains bounded. The next
      // geometry request must not run after its API context is uninstalled.
      void chrome.windows.update(id,{width:1,height:1}).catch(()=>{});
      void chrome.windows.update(id,{width:900,height:700,left:55,top:65}).catch(()=>{});
      await pause({phase:'context-cancel-ready',targetId:id,before:contextOriginal,requested:{width:900,height:700,left:55,top:65}});
    }
    window.addEventListener('message',event=>{
      if(event.data==='continue-window-update'){const resolve=resume;resume=null;resolve?.();}
      if(event.data==='run-window-update')run().catch(error=>{document.title=JSON.stringify({phase:'error',error:String(error)});});
    });
    document.title='window-update-ready';
  `;
  const file = PathUtils.join(
    PathUtils.tempDir,
    `window-update-${Services.uuid.generateUUID()}.xpi`,
  );
  const encode = (value: string) => new TextEncoder().encode(value);
  const gecko = marked
    ? {
      id: addonId,
      floorp_chrome_extension: { schema_version: 1, original_id: originalId },
    }
    : { id: addonId };
  let addon: WindowUpdateAddon | undefined;
  let extension: WindowUpdateExtension | undefined;
  let nativeWindow: WindowUpdateNativeWindow | undefined;
  let nativeRoot: EventTarget | undefined;
  const additionalWindows = new Set<WindowUpdateNativeWindow>();
  const nativeEvents: WindowUpdateNativeEvent[] = [];
  const reports: WindowUpdateReport[] = [];
  const onNativeEvent = (event: Event) => {
    if (!event.isTrusted || !nativeWindow) return;
    nativeEvents.push({
      type: event.type,
      at: Date.now(),
      width: nativeWindow.outerWidth,
      height: nativeWindow.outerHeight,
      left: nativeWindow.screenX,
      top: nativeWindow.screenY,
    });
  };
  const view = () =>
    [...(extension?.views ?? [])].find((value) =>
      value.viewType === "background" && value.xulBrowser
    )?.xulBrowser;
  const send = (message: string) => {
    const browser = view();
    assert(browser, "the real extension background exists");
    browser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `content.postMessage(${JSON.stringify(message)},'*');`,
        ),
      false,
    );
  };
  const phase = async (expected: string) => {
    const report = await waitFor(() => {
      const title = view()?.contentTitle;
      if (!title?.startsWith("{")) return undefined;
      const result = JSON.parse(title) as WindowUpdateReport;
      if (result.phase === "error") throw new Error(result.error);
      return result.phase === expected ? result : undefined;
    }, expected);
    reports.push(report);
    return report;
  };
  const matchesNative = (report: WindowUpdateReport) => {
    assert(
      report.returned && report.immediate && nativeWindow,
      "callback, get, and native widget snapshots exist",
    );
    for (const key of ["left", "top", "width", "height"] as const) {
      assertEquals(
        report.returned[key],
        report.immediate[key],
        `${key}: callback/get`,
      );
    }
    assertEquals(
      report.returned.width,
      nativeWindow.outerWidth,
      "callback/native width",
    );
    assertEquals(
      report.returned.height,
      nativeWindow.outerHeight,
      "callback/native height",
    );
    assertEquals(
      report.returned.left,
      nativeWindow.screenX,
      "callback/native left",
    );
    assertEquals(
      report.returned.top,
      nativeWindow.screenY,
      "callback/native top",
    );
  };
  try {
    await IOUtils.write(
      file,
      zipSync({
        "manifest.json": encode(JSON.stringify({
          manifest_version: 3,
          name: "Authored native window update callback contract",
          version: "1.0",
          background: { scripts: ["background.js"] },
          browser_specific_settings: { gecko },
        })),
        "background.js": encode(background),
      }),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(file));
    extension = await waitFor(
      () =>
        ExtensionParent.GlobalManager.extensionMap.get(addonId) as
          | WindowUpdateExtension
          | undefined,
      "extension startup",
    );
    await waitFor(
      () => view()?.contentTitle === "window-update-ready" ? true : undefined,
      "background startup",
    );
    send("run-window-update");
    const ready = await phase("ready");
    assert(
      ready.targetId !== undefined && ready.before,
      "created normal window is ready",
    );
    nativeWindow = extension.apiManager.global.windowTracker.getWindow(
      ready.targetId,
    );
    nativeRoot = nativeWindow.windowRoot;
    nativeWindow.addEventListener("resize", onNativeEvent);
    nativeRoot.addEventListener("MozUpdateWindowPos", onNativeEvent);
    send("continue-window-update");
    const normal = await phase("normal");
    if (marked) {
      matchesNative(normal);
      assertEquals(
        normal.returned?.width,
        normal.requested?.width,
        "normal callback width is current",
      );
      assertEquals(
        normal.returned?.height,
        normal.requested?.height,
        "normal callback height is current",
      );
      assert(
        nativeEvents.some((event) => event.type === "resize"),
        "a real trusted native resize was observed",
      );
    } else {
      assert(
        normal.returned && normal.immediate,
        "ordinary Firefox retains its Window result shape",
      );
      assertEquals(
        normal.returned.id,
        ready.targetId,
        "ordinary callback returns its target window",
      );
      await waitFor(
        () =>
          nativeWindow?.outerWidth === normal.requested?.width
            ? true
            : undefined,
        "ordinary native geometry eventually updates",
      );
    }
    send("continue-window-update");
    if (marked) {
      const queued = await phase("queued");
      matchesNative(queued);
      assertEquals(
        queued.returned?.left,
        queued.queuedFirst?.left,
        "queued top-only move preserves the acknowledged left",
      );
      send("continue-window-update");
      const clamped = await phase("clamped");
      matchesNative(clamped);
      if (clamped.returned?.width !== 1 || clamped.returned?.height !== 1) {
        assert(
          (clamped.elapsed ?? 0) >= 1900,
          "clamped nonmatching bounds use the explicit fallback, not an early resize event",
        );
      }
      send("continue-window-update");
      const ignored = await phase("possibly-ignored");
      matchesNative(ignored);
      if (
        ignored.returned?.width !== ignored.requested?.width ||
        ignored.returned?.height !== ignored.requested?.height
      ) {
        assert(
          (ignored.elapsed ?? 0) >= 1900,
          "ignored/nonmatching native bounds wait for the explicit fallback",
        );
      }
      send("continue-window-update");
      const closeReady = await phase("window-close-ready");
      assert(
        closeReady.targetId !== undefined && closeReady.before,
        "the disposable close target is ready",
      );
      const closingNative = extension.apiManager.global.windowTracker.getWindow(
        closeReady.targetId,
      );
      additionalWindows.add(closingNative);
      await waitFor(
        () =>
          closingNative.outerWidth !== closeReady.before!.width ||
            closingNative.outerHeight !== closeReady.before!.height
            ? true
            : undefined,
        "actual pending clamped geometry before target closure",
      );
      send("continue-window-update");
      const closed = await phase("window-closed");
      assert(closingNative.closed, "the real target window was closed");
      assert(
        /closed/i.test(closed.error ?? ""),
        "native close rejects the public callback",
      );
      send("continue-window-update");
      const cancelling = await phase("context-cancel-ready");
      assert(
        cancelling.before && cancelling.requested,
        "context cancellation has original and queued bounds",
      );
      await waitFor(() => {
        if (!nativeWindow) return undefined;
        return nativeWindow.outerWidth !== cancelling.before!.width ||
            nativeWindow.outerHeight !== cancelling.before!.height
          ? true
          : undefined;
      }, "actual clamped native request before context cancellation");
      const clampedBeforeUninstall = {
        width: nativeWindow.outerWidth,
        height: nativeWindow.outerHeight,
      };
      assert(
        addon,
        "the marked context is still installed before cancellation",
      );
      await addon.uninstall();
      addon = undefined;
      await sleep(2200);
      assertEquals(
        nativeWindow.outerWidth,
        clampedBeforeUninstall.width,
        "queued resize cannot run after context uninstall",
      );
      assertEquals(
        nativeWindow.outerHeight,
        clampedBeforeUninstall.height,
        "queued resize cannot run after context uninstall",
      );
    } else {
      await phase("done");
    }
    console.log(
      "[WindowUpdateNative]",
      JSON.stringify({ marked, reports, nativeEvents, nativeQualified: false }),
    );
  } finally {
    await addon?.uninstall();
    if (nativeWindow) {
      nativeWindow.removeEventListener("resize", onNativeEvent);
      nativeRoot?.removeEventListener("MozUpdateWindowPos", onNativeEvent);
      if (!nativeWindow.closed) nativeWindow.close();
    }
    for (const window of additionalWindows) {
      if (!window.closed) window.close();
    }
    await IOUtils.remove(file, { ignoreAbsent: true });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WindowUpdateNative", [
    {
      name:
        "marked native callback bounds, queued axes, fallback and context cancellation",
      fn: () => testWindowUpdate(true),
    },
    {
      name: "ordinary Firefox window update interface remains native",
      fn: () => testWindowUpdate(false),
    },
  ]);
}
