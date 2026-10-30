// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export interface FragmentSpeechEvent {
  type: string;
  charIndex: number;
  length?: number;
  receivedAt: number;
  elapsedTime?: number;
  name?: string;
}

export interface FragmentSpeechReport {
  ok: boolean;
  results: string[];
  cases: {
    name: string;
    text: string;
    events: FragmentSpeechEvent[];
  }[];
  voice: { voiceName: string; lang: string };
  error?: string;
  stack?: string;
}

export interface FragmentSpeechContext {
  xulBrowser: {
    contentTitle: string;
    messageManager: {
      loadFrameScript(source: string, delayed: boolean): void;
    };
  };
}

export interface FragmentSpeechModule {
  getAPI(context: unknown): { tts: { stop(): void } };
}

export interface FragmentSpeechExtension {
  backgroundContext: FragmentSpeechContext | null;
  apiManager: {
    asyncGetAPI(
      name: "tts",
      extension: FragmentSpeechExtension,
      scope: "addon_parent",
    ): Promise<FragmentSpeechModule>;
  };
}

export interface FragmentSpeechParent {
  GlobalManager: { extensionMap: Map<string, FragmentSpeechExtension> };
}

export interface FragmentNativeUtterance extends SpeechSynthesisUtterance {
  mozFloorpChromeTts: boolean;
}

export interface FragmentNativeEngine {
  window: {
    SpeechSynthesisUtterance: new (text: string) => FragmentNativeUtterance;
  };
  synth: SpeechSynthesis;
  active: object | null;
}
