// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zipSync } from "fflate/browser";
import type {
  BoundsAddon,
  BoundsBrowserWindow,
  BoundsChecksReport,
  BoundsWakeReport,
} from "./windows-test-types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function testWindowBounds(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const mainWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as BoundsBrowserWindow;
  const nativeOriginal = {
    width: mainWindow.outerWidth,
    height: mainWindow.outerHeight,
    left: mainWindow.screenX,
    top: mainWindow.screenY,
  };
  const id = "windows-bounds@floorp.test";
  const path = PathUtils.join(
    PathUtils.tempDir,
    `windows-${Services.uuid.generateUUID()}.xpi`,
  );
  const encode = (value: string) => new TextEncoder().encode(value);
  const background = `
    const events=[];
    const listener=window=>{events.push(window);document.title=JSON.stringify({phase:'event',events});};
    chrome.windows.onBoundsChanged.addListener(listener);
    const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
    let confirmResize;const resizeConfirmed=new Promise(resolve=>{confirmResize=resolve;});
    const wait=async predicate=>{for(let n=0;n<100;n++){if(predicate())return;await sleep(50);}throw new Error('bounds event timeout');};
    window.addEventListener('message',async event=>{
      if(event.data==='confirm-resize'){confirmResize();return;}
      if(event.data!=='run')return;
      const checks=[];
      const check=(name,value)=>{if(!value)throw new Error(name);checks.push(name);};
      const original=await chrome.windows.getCurrent();
      const waitGeometry=async expected=>{for(let n=0;n<100;n++){const value=await chrome.windows.get(original.id);if(Object.keys(expected).every(key=>value[key]===expected[key]))return value;await sleep(50);}throw new Error('native geometry did not settle: '+JSON.stringify(expected));};
      try {
        await sleep(400);
        check('registration does not emit initial bounds',events.length===0);
        const requested={width:original.width-80,height:original.height-60};
        await chrome.windows.update(original.id,requested);
        await wait(()=>events.some(value=>value.id===original.id&&value.width===requested.width&&value.height===requested.height));
        const changed=await waitGeometry(requested);
        check('real resize event includes native bounds',events.at(-1).id===original.id&&events.at(-1).width===changed.width&&events.at(-1).height===changed.height);
        document.title=JSON.stringify({phase:'resize-observed',checks,requested,resized:changed,event:events.at(-1)});
        await resizeConfirmed;
        check('window event excludes tab payload by default',!('tabs' in events.at(-1)));
        await sleep(400);
        const count=events.length;
        await chrome.windows.update(original.id,{width:changed.width,height:changed.height});
        await sleep(600);
        check('unchanged bounds do not emit',events.length===count);
        await chrome.windows.update(original.id,{left:35,top:45});
        await sleep(500);
        const moved=await chrome.windows.get(original.id);
        if(moved.left!==changed.left||moved.top!==changed.top){
          await wait(()=>events.length>count);
          check('native move event includes position',events.at(-1).left===moved.left&&events.at(-1).top===moved.top);
        }
        chrome.windows.onBoundsChanged.removeListener(listener);
        const removedCount=events.length;
        await chrome.windows.update(original.id,{width:changed.width+20});
        await waitGeometry({width:changed.width+20});
        await sleep(600);
        check('removeListener stops native observation',events.length===removedCount);
        const restored={left:original.left,top:original.top,width:original.width,height:original.height};
        await chrome.windows.update(original.id,restored);
        await waitGeometry(restored);
        await sleep(300);
        chrome.windows.onBoundsChanged.addListener(listener);
        document.title=JSON.stringify({phase:'done',checks});
      } catch(error){document.title=JSON.stringify({phase:'error',checks,error:String(error),stack:error.stack});}
    });
    document.title='windows-ready';
  `;
  let addon: BoundsAddon | undefined;
  const waitFor = async <T,>(
    read: () => T | false | undefined,
    label: string,
  ): Promise<T> => {
    const deadline = Date.now() + 15000;
    while (Date.now() < deadline) {
      const value = read();
      if (value) return value;
      await new Promise((resolve) => setTimeout(resolve, 30));
    }
    throw new Error(`Timeout: ${label}`);
  };
  try {
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encode(
          JSON.stringify({
            manifest_version: 3,
            name: "Native window bounds",
            version: "1.0",
            background: { scripts: ["background.js"] },
            browser_specific_settings: { gecko: { id } },
          }),
        ),
        "background.js": encode(background),
      }),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "extension",
    );
    const getView = () =>
      [...extension.views].find((view) =>
        view.viewType === "background" && view.xulBrowser
      );
    const view = await waitFor(() => {
      const value = getView();
      return value?.xulBrowser.contentTitle === "windows-ready" && value;
    }, "background");
    view.xulBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent("content.postMessage('run','*');"),
      false,
    );
    const resized = await waitFor(() => {
      const title = getView()?.xulBrowser.contentTitle;
      if (!title?.startsWith("{")) return false;
      const value = JSON.parse(title) as BoundsChecksReport;
      return ["resize-observed", "error"].includes(value.phase) && value;
    }, "native resize observation");
    assertEquals(
      resized.phase,
      "resize-observed",
      resized.error ?? "native resize observed",
    );
    assert(
      resized.requested && resized.resized && resized.event,
      "actual geometry snapshots are present",
    );
    assertEquals(
      resized.resized.width,
      resized.requested.width,
      "actual API width equals the requested width",
    );
    assertEquals(
      resized.resized.height,
      resized.requested.height,
      "actual API height equals the requested height",
    );
    assertEquals(
      resized.event.width,
      mainWindow.outerWidth,
      "event width equals the native widget width",
    );
    assertEquals(
      resized.event.height,
      mainWindow.outerHeight,
      "event height equals the native widget height",
    );
    view.xulBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent("content.postMessage('confirm-resize','*');"),
      false,
    );
    const report = await waitFor(() => {
      const title = getView()?.xulBrowser.contentTitle;
      if (!title?.startsWith("{")) return false;
      const value = JSON.parse(title) as BoundsChecksReport;
      return ["done", "error"].includes(value.phase) && value;
    }, "bounds checks");
    assertEquals(report.phase, "done", report.error ?? "bounds API report");
    assert(report.checks.length >= 5, "real bounds checks ran");
    const privateWindow = mainWindow.OpenBrowserWindow({ private: true });
    try {
      await waitFor(
        () => privateWindow.gBrowserInit?.delayedStartupFinished,
        "private window startup",
      );
      assert(
        !extension.canAccessWindow(privateWindow),
        "fixture lacks private-window permission",
      );
      privateWindow.resizeTo(
        privateWindow.outerWidth - 40,
        privateWindow.outerHeight - 30,
      );
      await new Promise((resolve) => setTimeout(resolve, 650));
      const privateTitle = getView()?.xulBrowser.contentTitle;
      assertEquals(
        JSON.parse(privateTitle).phase,
        "done",
        "private window geometry is not delivered",
      );
    } finally {
      privateWindow.close();
    }
    await extension.terminateBackground();
    const browserWindow = mainWindow;
    const oldWidth = browserWindow.outerWidth;
    browserWindow.resizeTo(oldWidth - 10, browserWindow.outerHeight);
    const wake = await waitFor(() => {
      const title = getView()?.xulBrowser.contentTitle;
      if (!title?.startsWith("{")) return false;
      const value = JSON.parse(title) as BoundsWakeReport;
      return value.phase === "event" && value;
    }, "persistent bounds listener wakes background");
    assertEquals(
      wake.events.at(-1)?.width,
      browserWindow.outerWidth,
      "woken event has native geometry",
    );
    browserWindow.resizeTo(oldWidth, browserWindow.outerHeight);
    console.log(
      "[WindowsNative]",
      JSON.stringify({ checks: report.checks, persistentWake: true }),
    );
  } finally {
    await addon?.uninstall();
    if (!mainWindow.closed) {
      mainWindow.resizeTo(nativeOriginal.width, nativeOriginal.height);
      mainWindow.moveTo(nativeOriginal.left, nativeOriginal.top);
    }
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WindowsNative", [{
    name: "native bounds changes, cleanup and persistent wake",
    fn: testWindowBounds,
  }]);
}
