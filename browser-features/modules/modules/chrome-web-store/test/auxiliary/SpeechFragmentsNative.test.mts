// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Linux SpeechDispatcher-specific fragmentation oracle; public TTS behavior on
// every native platform remains covered by AuxiliaryNative and waveform tests.
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type { ComponentAddonManager } from "../helpers/capture-component-test-types.ts";
import type {
  FragmentNativeEngine,
  FragmentSpeechEvent,
  FragmentSpeechParent,
  FragmentSpeechReport,
} from "../helpers/speech-fragment-test-types.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function until<T>(
  read: () => T | false | undefined | null,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 90000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw Error("Timed out waiting for " + description);
}

async function speechFragments(): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assertEquals(
    AppConstants.platform,
    "linux",
    "this regression targets the scoped Linux SpeechDispatcher backend",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "actual compiled extension bindings are required",
  );
  assert(
    Services.prefs.getBoolPref("floorp.chrome.tts.native", false),
    "actual native speech synthesis is required",
  );
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: ComponentAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: FragmentSpeechParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const original = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  const id = original + "@chromium-extension";
  const path = PathUtils.join(
    PathUtils.tempDir,
    "floorp-speech-fragments-" + Services.uuid.generateUUID() + ".xpi",
  );
  const background = `
const results=[],cases=[];
const check=(name,value)=>{if(!value)throw Error(name);results.push(name);};
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const wait=async(read,label)=>{for(let n=0;n<400;n++){if(read())return;await delay(50);}throw Error('Timed out: '+label);};
const collect=text=>({name:'',text,events:[]});
const eventFor=item=>event=>item.events.push({...event,receivedAt:Date.now()});
const terminal=event=>['end','interrupted','cancelled','error'].includes(event.type);
const verify=(item,ended=true)=>{
 check(item.name+' one start',item.events.filter(event=>event.type==='start').length===1);
 check(item.name+' one final event',item.events.filter(terminal).length===1);
 check(item.name+' final type',item.events.at(-1).type===(ended?'end':'interrupted'));
 if(ended)check(item.name+' original UTF16 end',item.events.at(-1).charIndex===item.text.length);
 const boundaries=item.events.filter(event=>event.type==='word'||event.type==='sentence');
 check(item.name+' native boundaries',boundaries.length>0);
 check(item.name+' monotonic UTF16 indices',boundaries.every((event,index)=>Number.isInteger(event.charIndex)&&event.charIndex>=0&&event.charIndex<item.text.length&&(!index||event.charIndex>=boundaries[index-1].charIndex)));
 check(item.name+' no split surrogate index',boundaries.every(event=>{const unit=item.text.charCodeAt(event.charIndex);return !(unit>=0xdc00&&unit<=0xdfff);}));
 check(item.name+' original UTF16 boundary lengths',boundaries.every(event=>{
  const end=event.charIndex+event.length,unit=item.text.charCodeAt(end);
  return Number.isInteger(event.length)&&event.length>0&&end<=item.text.length&&!(unit>=0xdc00&&unit<=0xdfff);
 }));
};
window.addEventListener('message',async event=>{
 if(event.data!=='run-native-speech-fragments')return;
 let voice;
 try{
  check('private native flag hidden',!('mozFloorpChromeTts' in new SpeechSynthesisUtterance('probe')));
  const voices=await chrome.tts.getVoices();voice=voices.find(item=>item.lang==='en-US')||voices.find(item=>item.lang.startsWith('en'));
  check('real installed English voice',!!voice);
  const speak=async item=>{
   await chrome.tts.speak(item.text,{voiceName:voice.voiceName,rate:2,onEvent:eventFor(item)});
   await wait(()=>item.events.some(terminal),item.name+' real end');
   verify(item);cases.push(item);
  };
  const cjk=collect('First native sentence。Second native sentence！Third native sentence？');cjk.name='CJK punctuation without spaces';await speak(cjk);
  const expected=[0,cjk.text.indexOf('Second'),cjk.text.indexOf('Third')];
  check('CJK sentence offsets from original string',JSON.stringify(cjk.events.filter(event=>event.type==='sentence').map(event=>event.charIndex))===JSON.stringify(expected));
  const scalar=collect('a'.repeat(255)+'😀'+'b'.repeat(30)+' trailing native words.');scalar.name='bounded unbroken UTF16 token';await speak(scalar);
  check('unbroken token length uses UTF16 units',scalar.events.find(event=>event.type==='word'&&event.charIndex===0)?.length===scalar.text.indexOf(' '));
  check('fragment does not invent word inside token',!scalar.events.some(event=>event.type==='word'&&event.charIndex>0&&event.charIndex<scalar.text.indexOf(' ')));
  const first=collect('First cancellable native sentence. Second retained native sentence. Third retained native sentence.');first.name='paused fragment cancellation';
  const queued=collect('Queued speech must never start.');queued.name='pending cancellation';
  await chrome.tts.speak(first.text,{voiceName:voice.voiceName,rate:1,onEvent:eventFor(first)});
  await wait(()=>first.events.some(event=>event.type==='start'),'actual first start');
  await wait(()=>first.events.some(event=>event.type==='word'&&event.charIndex>0),'actual native progress before pause');
  await chrome.tts.speak(queued.text,{voiceName:voice.voiceName,enqueue:true,onEvent:eventFor(queued)});
  chrome.tts.pause();await wait(()=>first.events.some(event=>event.type==='pause'),'native fragment pause');
  const pause=first.events.find(event=>event.type==='pause');
  check('pause retains unspoken fragments',pause.charIndex>0&&pause.charIndex<first.text.length);
  check('paused native task remains speaking',await chrome.tts.isSpeaking());
  const pausedCount=first.events.length;await delay(250);
  check('no callbacks while retained at pause',first.events.length===pausedCount);
  chrome.tts.resume();await wait(()=>first.events.some(event=>event.type==='resume'),'real resume');
  const resumeIndex=first.events.findIndex(event=>event.type==='resume');
  check('resume retains original paused offset',first.events[resumeIndex].charIndex===pause.charIndex);
  await wait(()=>first.events.slice(resumeIndex+1).some(event=>(event.type==='word'||event.type==='sentence')&&event.charIndex>pause.charIndex),'actual native progress after resume');
  chrome.tts.stop();await wait(()=>first.events.some(terminal)&&queued.events.some(terminal),'stop and queued cancellation');
  await delay(250);verify(first,false);cases.push(first,queued);
  check('queued task never starts',!queued.events.some(event=>event.type==='start'));
  check('queued task cancelled exactly once',queued.events.length===1&&queued.events[0].type==='cancelled');
  check('stop clears native state',!await chrome.tts.isSpeaking());
  const replacement=collect('A fresh native task still completes.');replacement.name='new task after cancellation';await speak(replacement);
  document.title=JSON.stringify({ok:true,results,cases,voice:{voiceName:voice.voiceName,lang:voice.lang}});
 }catch(error){chrome.tts.stop();document.title=JSON.stringify({ok:false,results,cases,error:String(error),stack:error.stack,voice});}
});
document.title='native-speech-fragments-ready';
`;
  const files = {
    "manifest.json": JSON.stringify({
      manifest_version: 3,
      name: "Native speech fragment regression fixture",
      version: "1.0",
      permissions: ["tts"],
      background: { scripts: ["background.js"] },
      browser_specific_settings: {
        gecko: {
          id,
          floorp_chrome_extension: { schema_version: 1, original_id: original },
        },
      },
    }),
    "background.js": background,
  };
  await IOUtils.write(
    path,
    zipSync(Object.fromEntries(
      Object.entries(files).map(([name, source]) => [
        name,
        new TextEncoder().encode(source),
      ]),
    )),
  );
  const addon = await AddonManager.installTemporaryAddon(
    new FileUtils.File(path),
  );
  try {
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "real extension startup",
    );
    const context = await until(
      () => extension.backgroundContext,
      "real background context",
    );
    await until(
      () => context.xulBrowser.contentTitle === "native-speech-fragments-ready",
      "actual public API fixture listener",
    );
    context.xulBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          'content.postMessage("run-native-speech-fragments","*");',
        ),
      false,
    );
    const report = await until(() => {
      const title = context.xulBrowser.contentTitle;
      return title.startsWith("{") && JSON.parse(title) as FragmentSpeechReport;
    }, "real native fragment, UTF16 and cancellation results");
    console.log("[SpeechFragmentsNative]", JSON.stringify(report));
    assert(report.ok, `${report.error}\n${report.stack}`);

    // A genuine native utterance created without Chrome's opt-in still uses
    // the ordinary Web Speech task. Its real speech/events are not substituted.
    const module = await extension.apiManager.asyncGetAPI(
      "tts",
      extension,
      "addon_parent",
    );
    const engine = Cu.evalInSandbox(
      "TtsEngine",
      Cu.getGlobalForObject(module.getAPI),
    ) as unknown as FragmentNativeEngine;
    assertEquals(engine.active, null, "Chrome task has fully completed");
    const ordinary = new engine.window.SpeechSynthesisUtterance(
      "Ordinary native web speech. Its legacy task still completes.",
    );
    assertEquals(
      ordinary.mozFloorpChromeTts,
      false,
      "ordinary native utterance retains the default legacy path",
    );
    ordinary.voice = engine.synth.getVoices().find((voice) =>
      voice.name === report.voice.voiceName
    ) ?? null;
    ordinary.rate = 2;
    const events: FragmentSpeechEvent[] = [];
    await new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => {
        engine.synth.cancel();
        reject(Error("Ordinary real Web Speech end timed out"));
      }, 20000);
      for (const type of ["start", "boundary", "end", "error"] as const) {
        ordinary.addEventListener(type, (event) => {
          events.push({
            type: event.type,
            charIndex: event.charIndex,
            elapsedTime: event.elapsedTime,
            name: event.name,
            receivedAt: Date.now(),
          });
          if (event.type === "end" || event.type === "error") {
            clearTimeout(timer);
            if (event.type === "error") {
              reject(Error("Ordinary Web Speech error"));
            } else resolve();
          }
        });
      }
      engine.synth.speak(ordinary);
    });
    assertEquals(
      events.filter((event) =>
        event.type === "start"
      ).length,
      1,
      "ordinary native task starts exactly once",
    );
    assertEquals(
      events.filter((event) => event.type === "end").length,
      1,
      "ordinary native task ends exactly once",
    );
    assertEquals(
      ordinary.mozFloorpChromeTts,
      false,
      "ordinary native speech never enables Chrome fragmentation",
    );
    console.log(
      "[SpeechFragmentsNative] ordinary native legacy",
      JSON.stringify(events),
    );
  } finally {
    await addon.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}

export function runAllTests(): Promise<void> {
  return runTests("SpeechFragmentsNative.test.mts", [{
    name: "real native CJK/UTF16 fragments preserve one task and cancellation",
    fn: speechFragments,
  }]);
}
