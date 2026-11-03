// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export interface AuxiliarySchemaService {
  load(url: string): Promise<void>;
  paramsValidationContexts: {
    get(
      params: { manifestVersion: number; cloneScope: typeof globalThis },
    ): unknown;
  };
  getNamespace(name: string): {
    get(name: string): {
      checkParameters(args: unknown[], context: unknown): unknown[];
    };
  };
}

export interface AuxiliaryTtsModule {
  getAPI(context: unknown): {
    tts: {
      speak(
        text: string,
        options: Record<string, unknown>,
        srcId: number,
      ): Promise<void>;
      stop(): void;
    };
  };
}

export interface AuxiliaryNativeUtterance {
  volume: number;
  pitch: number;
  rate: number;
  lang: string;
  mozFloorpChromeTts?: boolean;
}

export interface AuxiliaryOrdinaryApis {
  fontSettings: string;
  tts: string;
  privateSpeechOptIn: boolean;
}

export interface AuxiliaryTtsEngine {
  active: {
    utterance: AuxiliaryNativeUtterance;
    voice: AuxiliaryNativeVoice;
  } | null;
  synth: { getVoices(): AuxiliaryNativeVoice[] };
}

export interface AuxiliaryNativeVoice {
  name: string;
  lang: string;
  default: boolean;
}

export interface AuxiliaryOptionCase {
  input: Record<string, unknown>;
  expected: AuxiliaryNativeUtterance;
  selectedVoiceName?: string;
}
