// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
type NativeBrowser = {
  contentTitle: string;
  browsingContext: {
    currentWindowGlobal?: {
      documentPrincipal: { originAttributes: { privateBrowsingId: number } };
    };
  };
};
type NativeTab = { linkedBrowser: NativeBrowser };
type NativeWindow = Window & {
  OpenBrowserWindow(options?: { private: boolean }): NativeWindow;
  gBrowserInit?: { delayedStartupFinished: boolean };
  gBrowser: {
    selectedTab: NativeTab;
    addTab(uri: string, options: { triggeringPrincipal: unknown }): NativeTab;
    removeTab(tab: NativeTab): void;
  };
};
async function until<T>(
  read: () => T | false | undefined | Promise<T | false | undefined>,
  label: string,
): Promise<T> {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timeout: ${label}`);
}
async function privateFileSystem(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { ExtensionPermissions } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionPermissions.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const original = "jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj",
    id = original + "@chromium-extension";
  const permission = {
    permissions: ["internal:privateBrowsingAllowed"],
    origins: [],
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-private-fs-${Services.uuid.generateUUID()}.xpi`,
  );
  const main = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as NativeWindow;
  const windows: NativeWindow[] = [];
  const tabs: NativeTab[] = [];
  let addon: { uninstall(): Promise<void> } | undefined;
  const program = `
    const query=new URL(location.href).searchParams, mode=query.get('mode'),value=query.get('value');
    const call=operation=>new Promise((resolve,reject)=>operation(resolve,reject));
    const report=data=>document.title=JSON.stringify(data);
    (async()=>{try{
      const fs=await call((ok,no)=>webkitRequestFileSystem(PERSISTENT,1024,ok,no));
      if(mode==='pending'){
        let pending=200;
        const work=Array.from({length:200},(_,index)=>call((ok,no)=>fs.root.getFile('pending-'+index,{create:true},ok,no)).finally(()=>pending--));
        report({phase:'pending',pending});
        await Promise.all(work);report({phase:'pending-done'});return;
      }
      let entry;
      try{entry=await call((ok,no)=>fs.root.getFile('same.txt',{create:mode==='write'},ok,no));}
      catch(error){if(error.code===1){report({phase:'done',missing:true});return;}throw error;}
      if(mode==='write'){
        const writer=await call((ok,no)=>entry.createWriter(ok,no));
        await new Promise((resolve,reject)=>{writer.onwriteend=()=>writer.error?reject(writer.error):resolve();writer.write(new Blob([value]));});
      }
      const native=await call((ok,no)=>entry.file(ok,no));
      const fetched=await(await fetch(entry.toURL())).text();
      report({phase:'done',text:await native.text(),fetched,url:entry.toURL()});
    }catch(error){report({phase:'error',error:String(error),stack:error.stack});}})();`;
  const open = async (win: NativeWindow, mode: string, value = "") => {
    const tab = win.gBrowser.addTab(
      extension.baseURI.resolve(
        `private.html?mode=${mode}&value=${encodeURIComponent(value)}`,
      ),
      { triggeringPrincipal: extension.principal },
    );
    win.gBrowser.selectedTab = tab;
    if (win === main) tabs.push(tab);
    const result = await until(() => {
      const title = tab.linkedBrowser.contentTitle;
      if (!title?.startsWith("{")) return false;
      return JSON.parse(title) as {
        phase: string;
        missing?: boolean;
        text?: string;
        fetched?: string;
        pending?: number;
        error?: string;
      };
    }, `public FS ${mode}`);
    assert(
      result.phase !== "error",
      result.error ?? "native file system operation",
    );
    return { tab, result };
  };
  const privateWindow = async () => {
    const win = main.OpenBrowserWindow({ private: true });
    windows.push(win);
    await until(
      () => win.gBrowserInit?.delayedStartupFinished,
      "private window startup",
    );
    return win;
  };
  let extension: ReturnType<
    typeof ExtensionParent.GlobalManager.extensionMap.get
  >;
  try {
    await ExtensionPermissions.add(id, permission);
    const encode = (value: string) => new TextEncoder().encode(value);
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encode(JSON.stringify({
          manifest_version: 3,
          name: "Private native file system",
          version: "1.0",
          permissions: ["unlimitedStorage"],
          background: { scripts: ["background.js"] },
          browser_specific_settings: {
            gecko: {
              id,
              floorp_chrome_extension: {
                schema_version: 1,
                original_id: original,
              },
            },
          },
        })),
        "background.js": encode(
          "browser.runtime.onMessage.addListener(()=>Promise.resolve('ready'));",
        ),
        "private.html": encode(
          '<!doctype html><title>pending</title><script src="private.js"></script>',
        ),
        "private.js": encode(program),
      }),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "private-enabled addon",
    );
    assert(
      extension.privateBrowsingAllowed,
      "native private permission applied",
    );
    const regular = await open(main, "write", "regular-document");
    assertEquals(
      regular.result.text,
      "regular-document",
      "regular native File bytes",
    );
    assertEquals(
      regular.result.fetched,
      "regular-document",
      "regular filesystem URL bytes",
    );
    const first = await privateWindow();
    const missing = await open(first, "read");
    assertEquals(
      missing.tab.linkedBrowser.browsingContext.currentWindowGlobal
        ?.documentPrincipal.originAttributes.privateBrowsingId,
      1,
      "actual private document principal is PB1",
    );
    assert(missing.result.missing, "private session cannot read regular files");
    const privateWrite = await open(first, "write", "private-document");
    assertEquals(
      privateWrite.result.text,
      "private-document",
      "private native File write/read",
    );
    assertEquals(
      privateWrite.result.fetched,
      "private-document",
      "private filesystem protocol roundtrip",
    );
    assertEquals(
      (await open(main, "read")).result.text,
      "regular-document",
      "private writes cannot replace regular files",
    );
    const roots = (await IOUtils.getChildren(PathUtils.tempDir)).filter((p) =>
      PathUtils.filename(p).startsWith("floorp-chrome-filesystem-")
    );
    const storageRoot = await until(async () => {
      for (const root of roots) {
        if (
          await IOUtils.exists(
            PathUtils.join(root, original, "persistent", "data", "same.txt"),
          )
        ) return root;
      }
      return false;
    }, "actual private storage root");
    const pending = await open(first, "pending");
    assertEquals(
      pending.result.phase,
      "pending",
      "actual public file operations are pending at private exit",
    );
    assert(
      (pending.result.pending ?? 0) > 0,
      "private IO queue has outstanding operations",
    );
    first.close();
    await until(
      async () => !(await IOUtils.exists(storageRoot)),
      "last private exit removes private files after pending IO",
    );
    const second = await privateWindow();
    assert(
      (await open(second, "read")).result.missing,
      "new private session starts empty",
    );
    assertEquals(
      (await open(second, "write", "new-private-session")).result.fetched,
      "new-private-session",
      "new private generation can write and fetch",
    );
    second.close();
    await until(
      async () => !(await IOUtils.exists(storageRoot)),
      "second private cleanup",
    );
    await new Promise((resolve) => setTimeout(resolve, 400));
    assert(
      !(await IOUtils.exists(storageRoot)),
      "pending old operations cannot resurrect private files",
    );
    assertEquals(
      (await open(main, "read")).result.text,
      "regular-document",
      "private cleanup preserves regular storage",
    );
    console.log(
      "[FileSystemPrivateNative]",
      JSON.stringify({
        privatePrincipal: 1,
        protocol: true,
        isolation: true,
        pendingExit: true,
        newSession: true,
        noResurrection: true,
      }),
    );
  } finally {
    for (const win of windows) if (!win.closed) win.close();
    for (const tab of tabs) main.gBrowser.removeTab(tab);
    await addon?.uninstall();
    await ExtensionPermissions.remove(id, permission);
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("FileSystemPrivateNative", [{
    name: "real PB1 filesystem isolation, protocol and pending-IO cleanup",
    fn: privateFileSystem,
  }]);
}
