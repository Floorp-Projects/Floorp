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
async function until(read, label) {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw Error("Timed out waiting for " + label);
}
async function fileSystem() {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const original = "iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
    id = original + "@chromium-extension";
  const path = PathUtils.join(
    PathUtils.tempDir,
    "floorp-native-fs-" + Services.uuid.generateUUID() + ".xpi",
  );
  let addon;
  const extras = [], tabs = [];
  const foreignProbe = (url) =>
    `const image=()=>new Promise(resolve=>{const img=new Image();img.onload=()=>resolve(false);img.onerror=()=>resolve(true);img.src=${
      JSON.stringify(url)
    };});(async()=>{let fetchDenied=false,downloadDenied=false;try{await(await fetch(${
      JSON.stringify(url)
    })).arrayBuffer();}catch{fetchDenied=true;}try{await chrome.downloads.download({url:${
      JSON.stringify(url)
    },saveAs:false});}catch(error){downloadDenied=/Access denied/.test(error.message);}document.title='FSSEC:'+JSON.stringify({fetchDenied,downloadDenied,imageDenied:await image(),globalsAbsent:typeof webkitRequestFileSystem==='undefined'});})().catch(error=>document.title='FAIL:'+error);`;
  const blockedProbe =
    `const url=new URL(location.href).searchParams.get('url');const violations=[];document.addEventListener('securitypolicyviolation',event=>violations.push(event.effectiveDirective));const image=()=>new Promise(resolve=>{const img=new Image();img.onload=()=>resolve(false);img.onerror=()=>resolve(true);img.src=url;});(async()=>{let fetchDenied=false;try{await(await fetch(url)).arrayBuffer();}catch{fetchDenied=true;}const imageDenied=await image();await new Promise(resolve=>setTimeout(resolve,100));document.title='FSSEC:'+JSON.stringify({fetchDenied,imageDenied,violations});})().catch(error=>document.title='FAIL:'+error);`;
  const program =
    `const violations=[];document.addEventListener("securitypolicyviolation",event=>violations.push({directive:event.effectiveDirective,uri:event.blockedURI,policy:event.originalPolicy}));const promise=operation=>new Promise((resolve,reject)=>operation(resolve,reject));
 async function run(){try{
 const quota=await promise((ok,no)=>navigator.webkitPersistentStorage.requestQuota(64*1024*1024,ok,no));
 const fs=await promise((ok,no)=>webkitRequestFileSystem(PERSISTENT,1024*1024,ok,no));
 const dir=await promise((ok,no)=>fs.root.getDirectory('captures',{create:true},ok,no));
 const file=await promise((ok,no)=>dir.getFile('sample.txt',{create:true},ok,no));
 const writer=await promise((ok,no)=>file.createWriter(ok,no));
 const write=blob=>new Promise((resolve,reject)=>{writer.onwriteend=()=>writer.error?reject(writer.error):resolve();writer.write(blob);});
 await write(new Blob(['abcdef']));writer.seek(2);await write(new Blob(['XY']));
 await new Promise((resolve,reject)=>{writer.onwriteend=()=>writer.error?reject(writer.error):resolve();writer.truncate(5);});
 const nativeFile=await promise((ok,no)=>file.file(ok,no));const text=await nativeFile.text();
 const response=await fetch(file.toURL());const responseURL=response.url;const fetched=await response.text();
 const resolved=await promise((ok,no)=>webkitResolveLocalFileSystemURL(file.toURL(),ok,no));
 const entries=await promise((ok,no)=>dir.createReader().readEntries(ok,no));
 let traversal=false;try{await promise((ok,no)=>fs.root.getFile('../../outside',{create:true},ok,no));}catch(error){traversal=error.code===2;}
 const canvas=document.createElement('canvas');canvas.width=4;canvas.height=4;canvas.getContext('2d').fillStyle='rgb(20,200,80)';canvas.getContext('2d').fillRect(0,0,4,4);const png=await new Promise(resolve=>canvas.toBlob(resolve,'image/png'));
 const imageFile=await promise((ok,no)=>dir.getFile('actual.png',{create:true},ok,no));const imageWriter=await promise((ok,no)=>imageFile.createWriter(ok,no));
 await new Promise((resolve,reject)=>{imageWriter.onwriteend=()=>imageWriter.error?reject(imageWriter.error):resolve();imageWriter.write(png);});
 const image=new Image();const loaded=new Promise((resolve,reject)=>{image.onload=resolve;image.onerror=()=>reject(Error('filesystem PNG did not load'));});image.src=imageFile.toURL();await loaded;canvas.getContext('2d').drawImage(image,0,0);const pixel=Array.from(canvas.getContext('2d').getImageData(1,1,1,1).data);
 const large=await promise((ok,no)=>dir.getFile('large.bin',{create:true},ok,no));const largeWriter=await promise((ok,no)=>large.createWriter(ok,no));
 const bytes=new Uint8Array(16*1024*1024+8193);for(let index=0;index<bytes.length;index++)bytes[index]=(index*17)%251;
 await new Promise((resolve,reject)=>{largeWriter.onwriteend=()=>largeWriter.error?reject(largeWriter.error):resolve();largeWriter.write(new Blob([bytes]));});
 const largeBytes=await(await fetch(large.toURL())).arrayBuffer();const hash=async value=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',value))).map(v=>v.toString(16).padStart(2,'0')).join('');const largeIntegrity=largeBytes.byteLength===bytes.length&&await hash(largeBytes)===await hash(bytes);
 const cancelled=await fetch(large.toURL());const reader=cancelled.body.getReader();await reader.read();await reader.cancel();
 let malformedBuffer=false;try{await file._request('write',{bytes:2**31,position:0,job:'numeric-buffer'});}catch(error){malformedBuffer=error.code===8;}
 let oversizedBuffer=false;try{await file._request('write',{bytes:bytes.buffer,position:0,job:'oversized-buffer'});}catch(error){oversizedBuffer=error.code===8;}
 const tempQuota=await promise((ok,no)=>navigator.webkitTemporaryStorage.requestQuota(4,ok,no));const tempFS=await promise((ok,no)=>webkitRequestFileSystem(TEMPORARY,0,ok,no));const limited=await promise((ok,no)=>tempFS.root.getFile('limited.txt',{create:true},ok,no));const limitedWriter=await promise((ok,no)=>limited.createWriter(ok,no));let byteQuota=false;await new Promise(resolve=>{limitedWriter.onwriteend=()=>{byteQuota=limitedWriter.error?.code===FileError.QUOTA_EXCEEDED_ERR;resolve();};limitedWriter.write(new Blob(['12345']));});
 const usage=await new Promise((resolve,reject)=>navigator.webkitPersistentStorage.queryUsageAndQuota((used,quota)=>resolve({used,quota}),reject));
 document.title=JSON.stringify({quota,text,fetched,responseURL,largeIntegrity,malformedBuffer,oversizedBuffer,byteQuota,nativeFile:nativeFile instanceof File,resolved:resolved.fullPath,entries:entries.map(e=>e.name),traversal,pixel,url:imageFile.toURL(),usage});
 }catch(error){await new Promise(resolve=>setTimeout(resolve,300));document.title='FAIL:'+error.name+':'+error.message+':'+JSON.stringify(violations)+':'+error.stack;}}
 window.addEventListener('message',async event=>{try{const fs=await promise((ok,no)=>webkitRequestFileSystem(PERSISTENT,0,ok,no));if(event.data==='quota-probe'){let entryQuota=false,copyQuota=false;for(let i=0;i<12;i++){try{await promise((ok,no)=>fs.root.getFile('empty-'+i,{create:true},ok,no));}catch(error){entryQuota=error.code===FileError.QUOTA_EXCEEDED_ERR;break;}}const file=await promise((ok,no)=>fs.root.getFile('captures/sample.txt',{},ok,no));try{await promise((ok,no)=>file.copyTo(fs.root,'copied',ok,no));}catch(error){copyQuota=error.code===10;}document.title='FSSEC:'+JSON.stringify({entryQuota,copyQuota});}if(event.data==='pending-uninstall'){for(let i=0;i<100;i++)fs.root.getDirectory('pending-'+i,{create:true},()=>{},()=>{});fetch('filesystem:'+location.origin+'/persistent/captures/large.bin').then(response=>response.body.getReader().read()).catch(()=>{});document.title='FSSEC:'+JSON.stringify({pending:true});}}catch(error){document.title='FAIL:'+error;}});
 run();`;
  try {
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": new TextEncoder().encode(JSON.stringify({
          manifest_version: 3,
          name: "Native file system fixture",
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
        "background.js": new TextEncoder().encode(program),
        "blocked.html": new TextEncoder().encode(
          `<meta http-equiv="Content-Security-Policy" content="img-src 'none'; connect-src 'none'"><script src="blocked.js"></script>`,
        ),
        "blocked.js": new TextEncoder().encode(blockedProbe),
      }),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "file system addon",
    );
    const background = await until(
      () => extension.backgroundContext,
      "background",
    );
    await until(() => {
      const title = background.xulBrowser.contentTitle;
      if (title.startsWith("FAIL:")) throw Error(title);
      return title.startsWith("{");
    }, "real persistent filesystem write, fetch and image decode");
    const result = JSON.parse(background.xulBrowser.contentTitle);
    await IOUtils.writeUTF8(
      "/tmp/floorp-native-filesystem-result.json",
      JSON.stringify(result),
    );
    assertEquals(result.text, "abXYe", "seek and truncate write actual bytes");
    assertEquals(
      result.fetched,
      result.text,
      "filesystem URL reads stored bytes",
    );
    assert(result.nativeFile, "FileEntry.file returns a native File");
    assert(
      result.byteQuota,
      "native write exceeding granted byte quota fails before disk growth",
    );
    assert(result.traversal, "root escape rejected");
    assertEquals(
      JSON.stringify(result.pixel),
      JSON.stringify([20, 200, 80, 255]),
      "stored PNG decoded through filesystem protocol",
    );
    assert(result.usage.used > 16 * 1024 * 1024, "actual disk usage counted");
    assert(
      result.largeIntegrity,
      "more than one16MiB protocol chunk preserves every byte under backpressure",
    );
    assert(
      result.oversizedBuffer,
      "oversized native buffer rejected before IPC copy",
    );
    assert(
      result.malformedBuffer,
      "numeric pseudo-buffer rejected before parent allocation",
    );
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    const readProbe = (browser) =>
      until(() => {
        const title = browser.contentTitle;
        if (title.startsWith("FAIL:")) throw Error(title);
        return title.startsWith("FSSEC:") && JSON.parse(title.slice(6));
      }, "real origin/CSP probe");
    for (const marked of [true, false]) {
      const otherOriginal = marked
        ? "jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
        : "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk";
      const otherId = marked
        ? otherOriginal + "@chromium-extension"
        : "ordinary-filesystem-probe@example.test";
      const otherPath = PathUtils.join(
        PathUtils.tempDir,
        "foreign-fs-" + Services.uuid.generateUUID() + ".xpi",
      );
      extras.push(otherPath);
      await IOUtils.write(
        otherPath,
        zipSync({
          "manifest.json": new TextEncoder().encode(JSON.stringify({
            manifest_version: 3,
            name: "Foreign filesystem probe",
            version: "1.0",
            permissions: ["downloads"],
            background: { scripts: ["background.js"] },
            browser_specific_settings: {
              gecko: {
                id: otherId,
                ...marked
                  ? {
                    floorp_chrome_extension: {
                      schema_version: 1,
                      original_id: otherOriginal,
                    },
                  }
                  : {},
              },
            },
          })),
          "background.js": new TextEncoder().encode(foreignProbe(result.url)),
        }),
      );
      const other = await AddonManager.installTemporaryAddon(
        new FileUtils.File(otherPath),
      );
      extras.push(other);
      const otherExtension = await until(
        () => ExtensionParent.GlobalManager.extensionMap.get(otherId),
        "foreign addon",
      );
      const otherBackground = await until(
        () => otherExtension.backgroundContext,
        "foreign background",
      );
      const probe = await readProbe(otherBackground.xulBrowser);
      assert(
        probe.fetchDenied && probe.imageDenied && probe.downloadDenied,
        "different addon cannot fetch/decode owner image after owner cache priming",
      );
      assert(
        marked ? !probe.globalsAbsent : probe.globalsAbsent,
        "filesystem globals scope follows authenticated conversion marker",
      );
      const { NetUtil } = ChromeUtils.importESModule(
        "resource://gre/modules/NetUtil.sys.mjs",
      );
      for (
        const contentPolicyType of [
          Ci.nsIContentPolicy.TYPE_IMAGE,
          Ci.nsIContentPolicy.TYPE_SCRIPT,
        ]
      ) {
        let denied = false;
        try {
          NetUtil.newChannel({
            uri: result.url,
            loadingPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
            triggeringPrincipal: otherExtension.principal,
            securityFlags:
              Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
            contentPolicyType,
          });
        } catch {
          denied = true;
        }
        assert(
          denied,
          "system loader cannot proxy foreign triggering principal",
        );
      }
    }
    for (
      const url of [
        window.wrappedJSObject.__floorpRuntimeFixtureURL +
        "filesystem-security.html?url=" + encodeURIComponent(result.url),
        extension.baseURI.resolve(
          "blocked.html?url=" + encodeURIComponent(result.url),
        ),
      ]
    ) {
      const tab = window.gBrowser.addTab(url, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });
      tabs.push(tab);
      const probe = await readProbe(tab.linkedBrowser);
      assert(
        probe.fetchDenied && probe.imageDenied,
        "site origin and explicit owner CSP both reject primed filesystem resources: " +
          url + ": " + JSON.stringify(probe),
      );
      if (probe.violations) {
        assert(
          probe.violations.includes("connect-src") &&
            probe.violations.includes("img-src"),
          "native CSP directives remain enforced",
        );
      }
    }
    Services.prefs.setIntPref(
      "extensions.floorpChromeFileSystem.maxEntries",
      8,
    );
    const post = (command) =>
      background.xulBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            'content.document.title="WAIT";content.postMessage(' +
              JSON.stringify(command) + ',"*");',
          ),
        false,
      );
    post("quota-probe");
    const limits = await readProbe(background.xulBrowser);
    assert(
      limits.entryQuota && limits.copyQuota,
      "empty entries and copy obey bounded entry quota",
    );
    Services.prefs.clearUserPref(
      "extensions.floorpChromeFileSystem.maxEntries",
    );
    post("pending-uninstall");
    await readProbe(background.xulBrowser);
    await addon.uninstall();
    addon = null;
    const ownerRoot = PathUtils.join(
      PathUtils.profileDir,
      "floorp-chrome-filesystem",
      original,
    );
    await until(
      async () => !(await IOUtils.exists(ownerRoot)),
      "uninstall serializes queued filesystem work before removal",
    );
    await new Promise((resolve) => setTimeout(resolve, 300));
    assert(
      !(await IOUtils.exists(ownerRoot)),
      "pending work cannot recreate uninstalled storage",
    );
  } finally {
    Services.prefs.clearUserPref(
      "extensions.floorpChromeFileSystem.maxEntries",
    );
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    for (const tab of tabs) if (tab.isConnected) window.gBrowser.removeTab(tab);
    for (const extra of extras) {
      if (typeof extra === "string") {
        await IOUtils.remove(extra, { ignoreAbsent: true });
      } else await extra.uninstall();
    }
    await addon?.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}
export async function runAllTests() {
  await runTests("FileSystemNative.test.mts", [{
    name:
      "extension-owned persistent filesystem writes native files and loads actual URL bytes",
    fn: fileSystem,
  }]);
}
