// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../chrome/test/utils/test_harness.ts";
import { assertAuxiliaryDefaults } from "./auxiliary-defaults.mts";
import type {
  AuxiliaryOrdinaryApis,
  AuxiliaryTtsModule,
} from "../helpers/auxiliary-native-test-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

async function testNativeAuxiliary(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const originalId = "abcdefghijklmnopabcdefghijklmnop";
  const addonId = `${originalId}@chromium-extension`;
  const ordinaryId = "aux-ordinary@floorp.test";
  const nativeFonts = Services.prefs.getBoolPref(
    "floorp.chrome.fontSettings.native",
    false,
  );
  const nativeSpeech = Services.prefs.getBoolPref(
    "floorp.chrome.tts.native",
    false,
  );
  const requireNative =
    Services.env.get("FLOORP_REQUIRE_NATIVE_CHROME_AUXILIARY") === "1";
  if (requireNative) {
    assert(
      nativeFonts && nativeSpeech,
      "the packaged media candidate must include compiled font and speech backends",
    );
  }
  const background = `
  const results = [];
  let speechEvidence = null;
  const check = (name, value) => { if (!value) throw new Error(name); results.push(name); };
  const expectReject = async (name, action, match) => {
    try { await action(); } catch (error) { check(name, match.test(error.message)); return; }
    throw new Error(name + ': succeeded');
  };
  window.addEventListener('message', async event => {
    if (event.data !== 'run-auxiliary') return;
    try {
      check('fontSettings namespace', typeof chrome.fontSettings.getFontList === 'function');
      check('tts namespace', typeof chrome.tts.getVoices === 'function');
      check('private speech backend flag hidden', !('mozFloorpChromeTts' in new SpeechSynthesisUtterance()));
      const fonts = await chrome.fontSettings.getFontList();
      check('real installed fonts', fonts.length > 10 && fonts.every(f => f.fontId && f.displayName));
      const callbackFonts = await new Promise(resolve => chrome.fontSettings.getFontList(resolve));
      check('font callback matches Promise', callbackFonts.length === fonts.length);
      const fontWorker = new Worker(chrome.runtime.getURL('font-worker.js'));
      const workerMeasure = (font,text) => new Promise((resolve,reject) => {
        fontWorker.onmessage = event => resolve(event.data.width);
        fontWorker.onerror = event => reject(new Error(event.message));
        fontWorker.postMessage({font,text});
      });
      check('OffscreenCanvas Worker text shaping', await workerMeasure('20px serif','Floorp worker text') > 0);
      const changes = [];
      chrome.fontSettings.onDefaultFontSizeChanged.addListener(details => changes.push(details));
      for (const [name, value] of [['DefaultFontSize',23], ['DefaultFixedFontSize',19], ['MinimumFontSize',12]]) {
        const before = await chrome.fontSettings['get'+name]({});
        await chrome.fontSettings['set'+name]({pixelSize:value});
        const result = await chrome.fontSettings['get'+name]();
        check(name+' value/control', result.pixelSize === value && result.levelOfControl === 'controlled_by_this_extension');
        await chrome.fontSettings['clear'+name]();
        const after = await chrome.fontSettings['get'+name]();
        check(name+' restore', after.pixelSize === before.pixelSize && after.levelOfControl === 'controllable_by_this_extension');
      }
      await new Promise(resolve => setTimeout(resolve,50));
      check('font size real change events', changes.some(e => e.pixelSize === 23 && e.levelOfControl === 'controlled_by_this_extension'));
      if (${nativeFonts}) {
        const a = fonts.find(f => f.fontId === 'DejaVu Sans') || fonts[0];
        const b = fonts.find(f => f.fontId === 'DejaVu Serif') || fonts[1];
        const events = [];
        chrome.fontSettings.onFontChanged.addListener(e => events.push(e));
        await chrome.fontSettings.setFont({script:'Latn',genericFamily:'serif',fontId:a.fontId});
        await chrome.fontSettings.setFont({script:'Cyrl',genericFamily:'serif',fontId:b.fontId});
        check('Latin script value', (await chrome.fontSettings.getFont({script:'Latn',genericFamily:'serif'})).fontId === a.fontId);
        check('Worker Latin script uses real font', Math.abs(await workerMeasure('20px serif','Floorp text') - await workerMeasure('20px '+JSON.stringify(a.fontId),'Floorp text')) < 0.01);
        check('Worker Cyrillic script uses real font', Math.abs(await workerMeasure('20px serif','Кириллица') - await workerMeasure('20px '+JSON.stringify(b.fontId),'Кириллица')) < 0.01);
        check('Cyrillic separate value', (await chrome.fontSettings.getFont({script:'Cyrl',genericFamily:'serif'})).fontId === b.fontId);
        await chrome.fontSettings.setFont({genericFamily:'sansserif',fontId:a.fontId});
        check('global script value', (await chrome.fontSettings.getFont({genericFamily:'sansserif'})).fontId === a.fontId);
        await chrome.fontSettings.clearFont({script:'Latn',genericFamily:'serif'});
        await chrome.fontSettings.clearFont({script:'Cyrl',genericFamily:'serif'});
        await chrome.fontSettings.clearFont({genericFamily:'sansserif'});
        await new Promise(resolve => setTimeout(resolve,50));
        check('font events include script and family', events.some(e => e.script === 'Latn' && e.genericFamily === 'serif' && e.fontId === a.fontId));
      } else {
        await expectReject('uncompiled font setter rejects honestly', () => chrome.fontSettings.setFont({script:'Latn',genericFamily:'serif',fontId:fonts[0].fontId}), /compiled/);
      }
      fontWorker.terminate();
      let voices = await chrome.tts.getVoices();
      if (${requireNative}) {
        for(let attempt=0;attempt<80&&!voices.length;attempt++) {
          await new Promise(resolve=>setTimeout(resolve,250));
          voices = await chrome.tts.getVoices();
        }
      }
      const callbackVoices = await new Promise(resolve => chrome.tts.getVoices(resolve));
      check('voice callback matches Promise', voices.length === callbackVoices.length);
      check('idle isSpeaking false', await chrome.tts.isSpeaking() === false);
      if (!voices.length) {
        if (${requireNative}) throw new Error("Compiled native TTS requires a real operating-system synthesis voice");
        await expectReject('missing real voice rejects', () => chrome.tts.speak('Native synthesis test'), /native speech synthesis voice/);
      } else {
        check('real native voices', voices.every(v => v.voiceName && v.eventTypes.includes('start')));
        await expectReject('missing voice rejects', () => chrome.tts.speak('test',{voiceName:'nonexistent-floorp-test-voice'}), /unavailable/);
        await expectReject('required marker unsupported rejects', () => chrome.tts.speak('test',{requiredEventTypes:['marker']}), /required speech events/);
        await expectReject('ttsEngine extension rejected', () => chrome.tts.speak('test',{extensionId:'pppppppppppppppppppppppppppppppp'}), /speech engines/);
        await expectReject('utterance limit', () => chrome.tts.speak('x'.repeat(32769)), /32768/);
        const events = [];
        const speechStartedAt = Date.now();
        await new Promise(async (resolve,reject) => {
          const timeout=setTimeout(()=>reject(new Error('native speech end timeout')),20000);
          try {
            await chrome.tts.speak('Floorp native speech synthesis. This sentence has word boundaries.', {
              voiceName:voices.find(v=>v.lang.startsWith('en'))?.voiceName || voices[0].voiceName,
              onEvent(event) { events.push({...event, receivedAt:Date.now()}); if(event.type==='error'){clearTimeout(timeout);reject(new Error(event.errorMessage));}
                if(event.type==='end'){clearTimeout(timeout);resolve();} }
            });
            check('speaking state after accept', await chrome.tts.isSpeaking());
          } catch(error){clearTimeout(timeout);reject(error);}
        });
        speechEvidence={startedAt:speechStartedAt,completedAt:Date.now(),utterance:'Floorp native speech synthesis. This sentence has word boundaries.',events};
        check('real start/end callbacks', events[0]?.type === 'start' && events.at(-1)?.type === 'end');
        check('private routing fields hidden', events.every(e=>!('srcId' in e)&&!('isFinalEvent' in e)));
        check('completed isSpeaking false', await chrome.tts.isSpeaking() === false);
        if (${nativeSpeech}) {
          check('native word events', events.some(e=>e.type==='word'&&e.charIndex>0));
          check('native sentence events', events.some(e=>e.type==='sentence'));
          const first=[], queued=[], replacement=[];
          const long='This is a long native speech queue and cancellation test. '.repeat(30);
          await chrome.tts.speak(long,{rate:0.8,onEvent:e=>first.push(e)});
          await new Promise(resolve=>setTimeout(resolve,200));
          await chrome.tts.speak('Queued message.',{enqueue:true,onEvent:e=>queued.push(e)});
          chrome.tts.pause();
          for(let n=0;n<80&&!first.some(e=>e.type==='pause');n++) await new Promise(resolve=>setTimeout(resolve,100));
          check('pause event from native engine', first.some(e=>e.type==='pause'));
          check('paused remains speaking', await chrome.tts.isSpeaking());
          chrome.tts.resume();
          for(let n=0;n<80&&!first.some(e=>e.type==='resume');n++) await new Promise(resolve=>setTimeout(resolve,100));
          check('resume event from native engine', first.some(e=>e.type==='resume'));
          await chrome.tts.speak('Replacement message.',{onEvent:e=>replacement.push(e)});
          await new Promise(resolve=>setTimeout(resolve,100));
          check('active interrupted', first.some(e=>e.type==='interrupted'));
          check('pending cancelled', queued.some(e=>e.type==='cancelled'));
          chrome.tts.stop();
          await new Promise(resolve=>setTimeout(resolve,100));
          check('stop clears real state', await chrome.tts.isSpeaking() === false);
          check('replacement final event', replacement.some(e=>['interrupted','cancelled','end'].includes(e.type)));
        }
      }
      document.title = JSON.stringify({ok:true,results,voices:voices.length,nativeFonts:${nativeFonts},nativeSpeech:${nativeSpeech},speechEvidence});
    } catch(error) {document.title=JSON.stringify({ok:false,results,error:String(error),stack:error.stack});}
  });
  document.title='auxiliary-ready';
  `;
  const paths: string[] = [];
  const addons: Array<{ uninstall(): Promise<void> }> = [];
  const install = async (id: string, marked: boolean) => {
    const manifest = {
      manifest_version: 3,
      name: "Native auxiliary fixture",
      version: "1.0",
      permissions: ["fontSettings", "tts"],
      background: { scripts: ["background.js"] },
      browser_specific_settings: {
        gecko: {
          id,
          ...(marked
            ? {
              floorp_chrome_extension: {
                schema_version: 1,
                original_id: originalId,
              },
            }
            : {}),
        },
      },
    };
    const path = PathUtils.join(
      PathUtils.tempDir,
      `auxiliary-${Services.uuid.generateUUID()}.xpi`,
    );
    paths.push(path);
    const encode = (s: string) => new TextEncoder().encode(s);
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": encode(JSON.stringify(manifest)),
        "font-worker.js": encode(
          "onmessage=event=>{const context=new OffscreenCanvas(400,100).getContext('2d');context.font=event.data.font;postMessage({width:context.measureText(event.data.text).width});};",
        ),
        "background.js": encode(
          marked
            ? background
            : `document.title=JSON.stringify({fontSettings:typeof chrome.fontSettings,tts:typeof chrome.tts,privateSpeechOptIn:'mozFloorpChromeTts' in new SpeechSynthesisUtterance()});`,
        ),
      }),
    );
    addons.push(
      await AddonManager.installTemporaryAddon(new FileUtils.File(path)),
    );
    return id;
  };
  const waitFor = async <T,>(
    read: () => T | false | undefined,
    label: string,
  ): Promise<T> => {
    const deadline = Date.now() + 60000;
    while (Date.now() < deadline) {
      const value = read();
      if (value) return value;
      await new Promise((resolve) => setTimeout(resolve, 30));
    }
    throw new Error(`Timeout: ${label}`);
  };
  try {
    await install(ordinaryId, false);
    const ordinary = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(ordinaryId),
      "ordinary extension",
    );
    const ordinaryView = await waitFor(
      () =>
        [...ordinary.views].find((v) =>
          v.viewType === "background" &&
          v.xulBrowser?.contentTitle?.startsWith("{")
        ),
      "ordinary API isolation",
    );
    const ordinaryApis = JSON.parse(
      ordinaryView.xulBrowser.contentTitle,
    ) as AuxiliaryOrdinaryApis;
    assertEquals(
      ordinaryApis.fontSettings,
      "undefined",
      "ordinary Firefox extension has no fontSettings",
    );
    assertEquals(
      ordinaryApis.tts,
      "undefined",
      "ordinary Firefox extension has no tts",
    );
    assertEquals(
      ordinaryApis.privateSpeechOptIn,
      false,
      "ordinary Firefox extension cannot select the privileged native speech path",
    );
    await install(addonId, true);
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(addonId),
      "compatible extension",
    );
    const view = await waitFor(
      () =>
        [...extension.views].find((v) =>
          v.viewType === "background" &&
          v.xulBrowser?.contentTitle === "auxiliary-ready"
        ),
      "auxiliary background",
    );
    const parentTts = await extension.apiManager.asyncGetAPI(
      "tts",
      extension,
      "addon_parent",
    );
    try {
      await parentTts.getAPI(extension.backgroundContext).tts.getVoices();
    } catch (error) {
      console.error(
        "[AuxiliaryNative] parent TTS",
        String(error),
        (error as Error).stack,
      );
      throw error;
    }
    if (nativeSpeech) {
      await assertAuxiliaryDefaults(
        parentTts as unknown as AuxiliaryTtsModule,
        extension.backgroundContext,
      );
    }
    view.xulBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(`content.postMessage('run-auxiliary','*');`),
      false,
    );
    const report = await waitFor(() => {
      const title = view.xulBrowser.contentTitle;
      return title?.startsWith("{")
        ? JSON.parse(title) as {
          ok: boolean;
          results: string[];
          error?: string;
          stack?: string;
        }
        : false;
    }, "native auxiliary report");
    console.log("[AuxiliaryNative]", JSON.stringify(report));
    if (!report.ok) {
      console.error(
        "[AuxiliaryNative] native console",
        Services.console.getMessageArray().map((m) => m.message).filter((m) =>
          /tts|fontSettings|Unexpected|unexpected|Error/.test(m)
        ).slice(-40),
      );
    }
    assert(report.ok, `${report.error}\n${report.stack}`);
    assert(report.results.length >= 13, "native API checks ran");
    const fontsModule = await extension.apiManager.asyncGetAPI(
      "fontSettings",
      extension,
      "addon_parent",
    );
    const fontApi =
      fontsModule.getAPI(extension.backgroundContext).fontSettings;
    const browserWindow = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as unknown as
      | (Window & {
        InspectorUtils: {
          getUsedFontFaces(
            range: Range,
          ): Array<{ CSSFamilyName: string; name: string }>;
        };
      })
      | null;
    assert(browserWindow, "browser window exists for actual font rendering");
    const stage = browserWindow.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "span",
    );
    stage.style.cssText =
      "position:fixed;top:0;left:0;font-size:40px;opacity:0.01";
    browserWindow.document.documentElement.appendChild(stage);
    const usedFaces = (text: string, family: string) => {
      stage.textContent = text;
      stage.style.fontFamily = family;
      stage.getBoundingClientRect();
      const range = browserWindow.document.createRange();
      range.selectNodeContents(stage);
      return browserWindow.InspectorUtils.getUsedFontFaces(range).map((
        face,
      ) => ({
        family: face.CSSFamilyName,
        name: face.name,
      }));
    };
    const usedFonts = (text: string, family: string): string[] =>
      usedFaces(text, family).map((face) => face.family);
    try {
      const baseline = usedFonts("Latin sample", "serif");
      assert(
        baseline.length > 0,
        "real InspectorUtils observes actual glyph fonts",
      );
      if (nativeFonts) {
        const installedFonts = await fontApi.getFontList() as Array<
          { fontId: string }
        >;
        const available = new Set(installedFonts.map((font) => font.fontId));
        const candidates = Services.appinfo.OS === "Linux"
          ? [["DejaVu Sans", "DejaVu Serif"], ["Arial", "Times New Roman"]]
          : [["Arial", "Times New Roman"], ["DejaVu Sans", "DejaVu Serif"]];
        const inspect = (fontId: string) => ({
          fontId,
          latin: usedFaces("Latin sample", JSON.stringify(fontId)),
          cyrillic: usedFaces("Кириллица", JSON.stringify(fontId)),
        });
        const rendered = (font: ReturnType<typeof inspect>) =>
          [font.latin, font.cyrillic].every((faces) =>
            faces.length > 0 &&
            faces.every((face) =>
              face.family.toLowerCase() === font.fontId.toLowerCase() &&
              face.name
            )
          );
        const fingerprint = (faces: ReturnType<typeof usedFaces>) =>
          [...new Set(faces.map((face) => face.name))].sort().join("\n");
        const baselineFaces = usedFaces("Latin sample", "serif");
        const selected = candidates.filter((pair) =>
          pair.every((font) => available.has(font))
        )
          .map(([a, b]) => ({ a: inspect(a), b: inspect(b) }))
          .find((pair) =>
            rendered(pair.a) && rendered(pair.b) &&
            fingerprint(pair.a.latin) !== fingerprint(pair.b.latin) &&
            fingerprint(pair.a.cyrillic) !== fingerprint(pair.b.cyrillic) &&
            fingerprint(pair.a.latin) !== fingerprint(baselineFaces)
          );
        assert(
          selected,
          `Actual installed Latin/Cyrillic fonts must provide distinct glyph faces: ${
            JSON.stringify({
              os: Services.appinfo.OS,
              candidates,
              baselineFaces,
              availablePreferred: candidates.flat().filter((font) =>
                available.has(font)
              ),
            })
          }`,
        );
        const latinFont = selected.a.fontId, cyrillicFont = selected.b.fontId;
        console.log(
          "[AuxiliaryNative] actual font evidence",
          JSON.stringify({
            os: Services.appinfo.OS,
            installedFontCount: installedFonts.length,
            baselineFaces,
            latinOverride: selected.a,
            cyrillicOverride: selected.b,
          }),
        );
        await fontApi.setFont({
          script: "Latn",
          genericFamily: "serif",
          fontId: latinFont,
        });
        await fontApi.setFont({
          script: "Cyrl",
          genericFamily: "serif",
          fontId: cyrillicFont,
        });
        assert(
          usedFonts("Latin sample", "serif").some((name) => name === latinFont),
          "Latin override changes native glyph font",
        );
        assert(
          usedFonts("Кириллица", "serif").some((name) => name === cyrillicFont),
          "Cyrillic override retains independent native glyph font",
        );
        assert(
          usedFonts("Latin sample", JSON.stringify(cyrillicFont) + ", serif")
            .some((name) => name === cyrillicFont),
          "explicit page font retains CSS priority",
        );
        await fontApi.setFont({
          genericFamily: "fantasy",
          fontId: latinFont,
        });
        assert(
          usedFonts("Latin sample", "fantasy").some((name) =>
            name === latinFont
          ),
          "global script override changes generic native glyphs",
        );
        await fontApi.setFont({
          genericFamily: "standard",
          fontId: latinFont,
        });
        assert(
          usedFonts("Latin sample", "initial").some((name) =>
            name === latinFont
          ),
          "standard setting affects initial font family",
        );
        for (
          const [script, genericFamily] of [
            ["Latn", "serif"],
            ["Cyrl", "serif"],
            ["Zyyy", "fantasy"],
            ["Zyyy", "standard"],
          ]
        ) {
          await fontApi.clearFont({ script, genericFamily });
        }
        assertEquals(
          fingerprint(usedFaces("Latin sample", "serif")),
          fingerprint(baselineFaces),
          "clear restores the original native glyph faces",
        );
        assertEquals(
          usedFonts("Latin sample", "serif").join(","),
          baseline.join(","),
          "clear restores native glyph font",
        );
      }
      const prefs = Services.prefs.getChildList("font.size.variable.");
      const prior = prefs.map((name) => Services.prefs.getIntPref(name));
      await fontApi.setDefaultFontSize({ pixelSize: 27 });
      assert(
        prefs.every((name) => Services.prefs.getIntPref(name) === 27),
        "global font size changes every native language group",
      );
      const installed = await AddonManager.getAddonByID(addonId);
      await installed.disable();
      await waitFor(
        () =>
          prefs.every((name, index) =>
            Services.prefs.getIntPref(name) === prior[index]
          ),
        "disable restores each language group's prior value",
      );
      await installed.enable();
      console.log(
        "[AuxiliaryNative] native glyph and disable restoration checks passed",
        nativeFonts,
      );
    } finally {
      stage.remove();
    }
  } finally {
    for (const addon of addons.reverse()) await addon.uninstall();
    for (const path of paths) {
      await IOUtils.remove(path, { ignoreAbsent: true });
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("AuxiliaryNative", [{
    name:
      "actual fontSettings and TTS APIs use native preferences, fonts and speech",
    fn: testNativeAuxiliary,
  }]);
}
