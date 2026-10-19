// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";
import type {
  ComponentAddon,
  ComponentAddonManager,
  ComponentBrowser,
  ComponentExtensionParent,
  ComponentNativeWindow,
  ComponentTab,
  FileSystemProbe,
  FileSystemResult,
} from "../../helpers/capture-component-test-types.ts";
import type {
  ConcurrentFileSystemResult,
  PendingFileSystemWrites,
} from "./filesystem-concurrency-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until<T>(
  read: () =>
    | T
    | false
    | undefined
    | null
    | Promise<T | false | undefined | null>,
  label: string,
  timeoutMs = 20000,
): Promise<T> {
  const end = Date.now() + timeoutMs;
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
  ) as unknown as { AddonManager: ComponentAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: ComponentExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const original = "iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
    id = original + "@chromium-extension";
  const path = PathUtils.join(
    PathUtils.tempDir,
    "floorp-native-fs-" + Services.uuid.generateUUID() + ".xpi",
  );
  let addon: ComponentAddon | undefined | null;
  const extras: (string | ComponentAddon)[] = [], tabs: ComponentTab[] = [];
  const foreignProbe = (url: string) =>
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
 // Prepare every writer before starting any write: normal UI.Vision-style seeding
 // must accept more than four concurrent Blob writes without a transient quota error.
 const concurrent=await Promise.all(Array.from({length:13},async(_,index)=>{
   const entry=await promise((ok,no)=>dir.getFile('concurrent-'+index+'.bin',{create:true},ok,no));
   const writer=await promise((ok,no)=>entry.createWriter(ok,no));
   const size=index===11?16*1024*1024+8193:index===12?16*1024*1024+4097:256*1024+index*31;
   const source=new Uint8Array(size);for(let offset=0;offset<size;offset++)source[offset]=(offset*29+index*43+7)%251;
   const blob=new Blob([source]);const expectedSize=index===12?16*1024*1024:size;
   const expectedSHA256=await hash(source.subarray(0,expectedSize));source.fill(0);
   return {entry,writer,blob,expectedSize,expectedSHA256,events:[],progress:[]};
 }));
 const cancelledEntry=await promise((ok,no)=>dir.getFile('cancelled-seed.txt',{create:true},ok,no));
 const cancelledWriter=await promise((ok,no)=>cancelledEntry.createWriter(ok,no));
 await new Promise((resolve,reject)=>{cancelledWriter.onwriteend=()=>cancelledWriter.error?reject(cancelledWriter.error):resolve();cancelledWriter.write(new Blob(['queued-seed']));});cancelledWriter.seek(0);
 const concurrentWrites=concurrent.map((item,index)=>new Promise((resolve,reject)=>{
   for(const name of ['writestart','write','abort','error'])item.writer['on'+name]=()=>item.events.push(name);
   item.writer.onprogress=event=>{item.events.push('progress');item.progress.push({loaded:event.loaded,total:event.total});if(index===12&&event.loaded===16*1024*1024)item.writer.abort();};
   item.writer.onwriteend=()=>{item.events.push('writeend');if(index!==12&&item.writer.error)reject(item.writer.error);else resolve();};
   item.writer.write(item.blob);
 }));
 const cancelledEvents=[];
 const cancelledWrite=new Promise(resolve=>{
   for(const name of ['writestart','progress','write','abort','error'])cancelledWriter['on'+name]=()=>cancelledEvents.push(name);
   cancelledWriter.onwriteend=()=>{cancelledEvents.push('writeend');resolve();};
   cancelledWriter.write(new Blob(['unwanted queued replacement']));
 });
 const pendingAtStart=concurrent.filter(item=>item.writer.readyState===FileWriter.WRITING).length;
 await Promise.resolve();const stateBeforeAbort=cancelledWriter.readyState;cancelledWriter.abort();
 await Promise.all([...concurrentWrites,cancelledWrite]);
 const cancelledNative=await promise((ok,no)=>cancelledEntry.file(ok,no));
 const cancelledResult={stateBeforeAbort,state:cancelledWriter.readyState,errorCode:cancelledWriter.error?.code??null,
   events:[...cancelledEvents],position:cancelledWriter.position,length:cancelledWriter.length,
   text:await cancelledNative.text(),fetched:await(await fetch(cancelledEntry.toURL())).text()};
 for(const name of ['writestart','progress','write','abort','error'])cancelledWriter['on'+name]=null;
 cancelledWriter.seek(0);
 await new Promise((resolve,reject)=>{cancelledWriter.onwriteend=()=>cancelledWriter.error?reject(cancelledWriter.error):resolve();cancelledWriter.write(new Blob(['reused queued writer']));});
 const reusedNative=await promise((ok,no)=>cancelledEntry.file(ok,no));
 const cancelledReuse={text:await reusedNative.text(),fetched:await(await fetch(cancelledEntry.toURL())).text(),state:cancelledWriter.readyState,errorCode:cancelledWriter.error?.code??null};
 const records=await Promise.all(concurrent.map(async(item,index)=>{
   const native=await promise((ok,no)=>item.entry.file(ok,no));const stored=await native.arrayBuffer();
   const fetched=await(await fetch(item.entry.toURL())).arrayBuffer();
   return {name:item.entry.name,expectedSize:item.expectedSize,expectedSHA256:item.expectedSHA256,
     fileSize:native.size,fileSHA256:await hash(stored),fetchSize:fetched.byteLength,fetchSHA256:await hash(fetched),
     nativeFile:native instanceof File,events:[...item.events],progress:item.progress.map(value=>({...value})),state:item.writer.readyState,
     position:item.writer.position,length:item.writer.length,errorCode:item.writer.error?.code??null};
 }));
 // Subsequent public overwrite/truncate must be visible to a newly obtained File.
 const replacement='replacement after queued writes';const rewriteWriter=concurrent[0].writer;rewriteWriter.seek(0);
 await new Promise((resolve,reject)=>{rewriteWriter.onprogress=null;rewriteWriter.onwriteend=()=>rewriteWriter.error?reject(rewriteWriter.error):resolve();rewriteWriter.write(new Blob([replacement]));});
 await new Promise((resolve,reject)=>{rewriteWriter.onwriteend=()=>rewriteWriter.error?reject(rewriteWriter.error):resolve();rewriteWriter.truncate(replacement.length);});
 const current=await promise((ok,no)=>concurrent[0].entry.file(ok,no));
 const concurrency={pendingAtStart,records,cancelled:cancelledResult,reused:cancelledReuse,rewritten:{size:current.size,nativeFile:current instanceof File,text:await current.text(),fetched:await(await fetch(concurrent[0].entry.toURL())).text()}};
 const cancelled=await fetch(large.toURL());const reader=cancelled.body.getReader();await reader.read();await reader.cancel();
 let malformedBuffer=false;try{await file._request('write',{bytes:2**31,position:0,job:'numeric-buffer'});}catch(error){malformedBuffer=error.code===8;}
 let oversizedBuffer=false;try{await file._request('write',{bytes:bytes.buffer,position:0,job:'oversized-buffer'});}catch(error){oversizedBuffer=error.code===8;}
 const tempQuota=await promise((ok,no)=>navigator.webkitTemporaryStorage.requestQuota(4,ok,no));const tempFS=await promise((ok,no)=>webkitRequestFileSystem(TEMPORARY,0,ok,no));const limited=await promise((ok,no)=>tempFS.root.getFile('limited.txt',{create:true},ok,no));const limitedWriter=await promise((ok,no)=>limited.createWriter(ok,no));let byteQuota=false;await new Promise(resolve=>{limitedWriter.onwriteend=()=>{byteQuota=limitedWriter.error?.code===FileError.QUOTA_EXCEEDED_ERR;resolve();};limitedWriter.write(new Blob(['12345']));});
 const usage=await new Promise((resolve,reject)=>navigator.webkitPersistentStorage.queryUsageAndQuota((used,quota)=>resolve({used,quota}),reject));
 document.title=JSON.stringify({quota,text,fetched,responseURL,largeIntegrity,concurrency,malformedBuffer,oversizedBuffer,byteQuota,nativeFile:nativeFile instanceof File,resolved:resolved.fullPath,entries:entries.map(e=>e.name),traversal,pixel,url:imageFile.toURL(),usage});
 }catch(error){await new Promise(resolve=>setTimeout(resolve,300));document.title='FAIL:'+error.name+':'+error.message+':'+JSON.stringify(violations)+':'+error.stack;}}
 window.addEventListener('message',async event=>{try{const fs=await promise((ok,no)=>webkitRequestFileSystem(PERSISTENT,0,ok,no));if(event.data==='quota-probe'){let entryQuota=false,copyQuota=false;for(let i=0;i<12;i++){try{await promise((ok,no)=>fs.root.getFile('empty-'+i,{create:true},ok,no));}catch(error){entryQuota=error.code===FileError.QUOTA_EXCEEDED_ERR;break;}}const file=await promise((ok,no)=>fs.root.getFile('captures/sample.txt',{},ok,no));try{await promise((ok,no)=>file.copyTo(fs.root,'copied',ok,no));}catch(error){copyQuota=error.code===10;}document.title='FSSEC:'+JSON.stringify({entryQuota,copyQuota});}if(event.data==='pending-uninstall'){const writers=await Promise.all(Array.from({length:12},async(_,index)=>{const entry=await promise((ok,no)=>fs.root.getFile('captures/concurrent-'+index+'.bin',{},ok,no));return promise((ok,no)=>entry.createWriter(ok,no));}));const blob=new Blob([new Uint8Array(256*1024)]);for(const writer of writers){writer.onwriteend=()=>{};writer.write(blob);}const pendingWrites=writers.filter(writer=>writer.readyState===FileWriter.WRITING).length;for(let i=0;i<100;i++)fs.root.getDirectory('pending-'+i,{create:true},()=>{},()=>{});fetch('filesystem:'+location.origin+'/persistent/captures/large.bin').then(response=>response.body.getReader().read()).catch(()=>{});document.title='FSSEC:'+JSON.stringify({pending:true,pendingWrites});}}catch(error){document.title='FAIL:'+error;}});
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
      () =>
        extension.backgroundContext?.xulBrowser && extension.backgroundContext,
      "actual file system background document browser",
    );
    await until(
      () => {
        const title = background.xulBrowser?.contentTitle;
        if (!title) return false;
        if (title.startsWith("FAIL:")) throw Error(title);
        return title.startsWith("{");
      },
      "real persistent filesystem write, concurrent native IO, fetch and image decode",
      60000,
    );
    const result = JSON.parse(
      background.xulBrowser.contentTitle,
    ) as FileSystemResult & ConcurrentFileSystemResult;
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
    assertEquals(
      result.concurrency.pendingAtStart,
      13,
      "all thirteen public writers start before waiting for completion",
    );
    assertEquals(
      result.concurrency.records.length,
      13,
      "each concurrent writer has persisted-content evidence",
    );
    for (const [index, record] of result.concurrency.records.entries()) {
      assertEquals(
        record.name,
        `concurrent-${index}.bin`,
        "each writer retains its own entry",
      );
      assertEquals(
        record.expectedSize,
        index === 11
          ? 16 * 1024 * 1024 + 8193
          : index === 12
          ? 16 * 1024 * 1024
          : 256 * 1024 + index * 31,
        `${record.name}: authored payload size`,
      );
      assert(
        record.nativeFile,
        "concurrent FileEntry.file returns a native File",
      );
      assertEquals(
        record.fileSize,
        record.expectedSize,
        `${record.name}: native persisted byte count`,
      );
      assertEquals(
        record.fetchSize,
        record.expectedSize,
        `${record.name}: filesystem protocol byte count`,
      );
      assertEquals(
        record.fileSHA256,
        record.expectedSHA256,
        `${record.name}: exact Blob snapshot persists through native IO`,
      );
      assertEquals(
        record.fetchSHA256,
        record.expectedSHA256,
        `${record.name}: filesystem protocol preserves every persisted byte`,
      );
      assertEquals(record.state, 2, `${record.name}: writer reaches DONE`);
      assertEquals(
        record.position,
        record.expectedSize,
        `${record.name}: writer position reflects persisted bytes`,
      );
      assertEquals(
        record.length,
        record.expectedSize,
        `${record.name}: writer length reflects persisted bytes`,
      );
      assertEquals(
        record.errorCode,
        index === 12 ? 3 : null,
        `${record.name}: only deliberate abort reports ABORT_ERR`,
      );
      const events = index === 12
        ? ["writestart", "progress", "abort", "writeend"]
        : index === 11
        ? ["writestart", "progress", "progress", "write", "writeend"]
        : ["writestart", "progress", "write", "writeend"];
      assertEquals(
        JSON.stringify(record.events),
        JSON.stringify(events),
        `${record.name}: actual completion and abort event order`,
      );
      const total = index === 12
        ? 16 * 1024 * 1024 + 4097
        : record.expectedSize;
      const loaded = index === 11
        ? [16 * 1024 * 1024, total]
        : [record.expectedSize];
      assertEquals(
        JSON.stringify(record.progress),
        JSON.stringify(loaded.map((value) => ({ loaded: value, total }))),
        `${record.name}: chunk progress counts actual persisted bytes`,
      );
    }
    const cancelled = result.concurrency.cancelled;
    assertEquals(
      cancelled.stateBeforeAbort,
      1,
      "cancelled writer is publicly WRITING after all fourteen writes start",
    );
    assertEquals(cancelled.state, 2, "cancelled waiter reaches DONE");
    assertEquals(cancelled.errorCode, 3, "cancelled waiter reports ABORT_ERR");
    assertEquals(
      JSON.stringify(
        cancelled.events.filter((event) => event !== "writestart"),
      ),
      JSON.stringify(["abort", "writeend"]),
      "cancelled waiter has one abort and writeend without progress, success or error events",
    );
    assert(
      cancelled.events.filter((event) => event === "writestart").length <= 1,
      "cancelled waiter cannot start twice",
    );
    assertEquals(
      cancelled.position,
      0,
      "cancelled waiter cannot advance file position",
    );
    assertEquals(
      cancelled.length,
      "queued-seed".length,
      "cancelled waiter preserves file length",
    );
    assertEquals(
      cancelled.text,
      "queued-seed",
      "cancelled waiter preserves exact native seed bytes",
    );
    assertEquals(
      cancelled.fetched,
      "queued-seed",
      "cancelled waiter preserves exact filesystem fetch bytes",
    );
    assertEquals(
      result.concurrency.reused.text,
      "reused queued writer",
      "cancelled writer can subsequently persist exact native bytes",
    );
    assertEquals(
      result.concurrency.reused.fetched,
      "reused queued writer",
      "cancelled writer can subsequently serve exact filesystem bytes",
    );
    assertEquals(
      result.concurrency.reused.state,
      2,
      "reused writer reaches DONE",
    );
    assertEquals(
      result.concurrency.reused.errorCode,
      null,
      "subsequent successful write clears abort error",
    );
    assert(
      result.concurrency.rewritten.nativeFile,
      "new FileEntry.file returns a native File after overwrite and truncate",
    );
    assertEquals(
      result.concurrency.rewritten.size,
      "replacement after queued writes".length,
      "new FileEntry.file observes exact truncated replacement size",
    );
    assertEquals(
      result.concurrency.rewritten.text,
      "replacement after queued writes",
      "subsequent native File observes the replacement",
    );
    assertEquals(
      result.concurrency.rewritten.fetched,
      "replacement after queued writes",
      "subsequent filesystem fetch observes the replacement",
    );
    const window = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as unknown as ComponentNativeWindow;
    const readProbe = (browser: ComponentBrowser) =>
      until(() => {
        const title = browser.contentTitle;
        if (title.startsWith("FAIL:")) throw Error(title);
        return title.startsWith("FSSEC:") &&
          JSON.parse(title.slice(6)) as FileSystemProbe;
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
        () =>
          otherExtension.backgroundContext?.xulBrowser &&
          otherExtension.backgroundContext,
        "actual foreign addon background document browser",
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
    const post = (command: string) =>
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
    const pending = await until(async () => {
      const observed = await readProbe(background.xulBrowser);
      return observed.pending === true && observed;
    }, "actual pending-uninstall public marker") as
      & FileSystemProbe
      & PendingFileSystemWrites;
    assertEquals(
      pending.pendingWrites,
      12,
      "actual public writes are pending when uninstall begins",
    );
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
    const window = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as unknown as ComponentNativeWindow;
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
