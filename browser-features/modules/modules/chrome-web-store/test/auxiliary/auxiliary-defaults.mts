// SPDX-License-Identifier: MPL-2.0
// Native schema normalization and actual engine option regression guards.
import {
  assert,
  assertEquals,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type {
  AuxiliaryOptionCase,
  AuxiliarySchemaService,
  AuxiliaryTtsEngine,
  AuxiliaryTtsModule,
} from "../helpers/auxiliary-native-test-types.ts";

export async function assertAuxiliaryDefaults(
  module: AuxiliaryTtsModule,
  context: unknown,
): Promise<void> {
  const { Schemas } = ChromeUtils.importESModule(
    "resource://gre/modules/Schemas.sys.mjs",
  ) as unknown as { Schemas: AuxiliarySchemaService };
  await Schemas.load("chrome://extensions/content/schemas/font_settings.json");
  await Schemas.load("chrome://extensions/content/schemas/tts.json");
  const validation = Schemas.paramsValidationContexts.get({
    manifestVersion: 3,
    cloneScope: globalThis,
  });
  const normalized = (namespace: string, name: string, args: unknown[]) =>
    Schemas.getNamespace(namespace).get(name).checkParameters(args, validation);
  const detailsAt = (
    args: unknown[],
    index: number,
  ): Record<string, unknown> => {
    const details = args[index];
    assert(details && typeof details === "object", "normalized details exist");
    return details as Record<string, unknown>;
  };
  for (const name of ["getFont", "setFont", "clearFont"]) {
    const details = {
      genericFamily: "sansserif",
      ...(name === "setFont" ? { fontId: "DejaVu Sans" } : {}),
    };
    for (const script of [undefined, null, "Latn", "Cyrl"]) {
      const value = detailsAt(
        normalized("fontSettings", name, [{
          ...details,
          ...(script === undefined ? {} : { script }),
        }]),
        0,
      );
      assertEquals(value.script, script ?? "Zyyy", `${name} script default`);
    }
    let rejected = false;
    try {
      normalized("fontSettings", name, [{
        ...details,
        script: "invalid-script",
      }]);
    } catch {
      rejected = true;
    }
    assert(rejected, `${name} rejects invalid script`);
  }

  // Inspect the real module's native utterance, without replacing its engine,
  // callbacks, or audio route. The public suite below still verifies speech.
  const engine = Cu.evalInSandbox(
    "TtsEngine",
    Cu.getGlobalForObject(module.getAPI),
  ) as unknown as AuxiliaryTtsEngine;
  const api = module.getAPI(context).tts;
  const voices = engine.synth.getVoices();
  assert(
    voices.length > 0,
    "native voice selection uses actual installed voices",
  );
  const locale = Services.locale.appLocaleAsBCP47.toLowerCase();
  const defaultVoice = voices.find((voice) => voice.default);
  const evidence = [];
  const cases: AuxiliaryOptionCase[] = [
    { input: {}, expected: { volume: 1, pitch: 1, rate: 1, lang: "" } },
    {
      input: { volume: null, pitch: null, rate: null, lang: null },
      expected: { volume: 1, pitch: 1, rate: 1, lang: "" },
    },
    {
      input: { volume: 0, pitch: 0, rate: 0.8, lang: "en" },
      expected: { volume: 0, pitch: 0, rate: 0.8, lang: "en" },
    },
    {
      input: { volume: 0.6, pitch: 1.2, rate: 1.25, lang: "en" },
      expected: { volume: 0.6, pitch: 1.2, rate: 1.25, lang: "en" },
    },
    {
      input: { voiceName: voices.at(-1)!.name, lang: "en" },
      expected: { volume: 1, pitch: 1, rate: 1, lang: "en" },
      selectedVoiceName: voices.at(-1)!.name,
    },
  ];
  for (
    const [index, { input, expected, selectedVoiceName }] of cases.entries()
  ) {
    const options = detailsAt(
      normalized("tts", "speak", [
        "Native utterance default verification.",
        input,
      ]),
      1,
    );
    try {
      await api.speak(
        "Native utterance default verification.",
        options,
        -100 - index,
      );
      const utterance = engine.active?.utterance;
      assert(utterance, "the actual TTS engine created a native utterance");
      assertEquals(
        utterance.mozFloorpChromeTts,
        true,
        "the privileged Chrome TTS host selects the scoped native speech path",
      );
      const voice = engine.active?.voice;
      assert(voice, "native TTS selected an actual installed voice");
      if (index === 0) {
        if (defaultVoice) {
          assertEquals(
            voice.name,
            defaultVoice.name,
            "native default voice has priority",
          );
        } else if (
          voices.some((candidate) => candidate.lang.toLowerCase() === locale)
        ) {
          assertEquals(
            voice.lang.toLowerCase(),
            locale,
            "default voice matches actual application locale",
          );
        } else if (
          voices.some((candidate) =>
            candidate.lang.toLowerCase().split("-")[0] === locale.split("-")[0]
          )
        ) {
          assertEquals(
            voice.lang.toLowerCase().split("-")[0],
            locale.split("-")[0],
            "default voice matches application base language",
          );
        }
      }
      if (selectedVoiceName) {
        assertEquals(
          voice.name,
          selectedVoiceName,
          "explicit voice retains priority over language fallback",
        );
      }
      const actual = {
        volume: utterance.volume,
        pitch: utterance.pitch,
        rate: utterance.rate,
        lang: utterance.lang,
      };
      for (const key of ["volume", "pitch", "rate"] as const) {
        assert(
          Math.abs(actual[key] - expected[key]) < 0.00001,
          `native ${key} default/value`,
        );
      }
      assertEquals(actual.lang, expected.lang, "native language default/value");
      evidence.push({
        input,
        actual,
        selectedVoice: {
          name: voice.name,
          lang: voice.lang,
          default: voice.default,
        },
      });
    } finally {
      api.stop();
    }
  }
  console.log(
    "[AuxiliaryNative] actual schema and native option defaults",
    JSON.stringify(evidence),
  );
  // The external recorder measures a 750 ms baseline before the public
  // utterance. Let any genuine output tail from these stopped probes drain.
  const { setTimeout } = ChromeUtils.importESModule(
    "resource://gre/modules/Timer.sys.mjs",
  );
  await new Promise((resolve) => setTimeout(resolve, 1000));
}
