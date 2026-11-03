// SPDX-License-Identifier: MPL-2.0
import type { TtsValidationWindow } from "./tts-validation-test-types.ts";

export type ContentTimerMethod =
  | "setTimeout"
  | "clearTimeout"
  | "setInterval"
  | "clearInterval";
export type ContentTimerMode = "converted" | "ordinary" | "mismatched";
export interface ContentTimerCall {
  succeeded: boolean;
  idType?: string;
  name?: string;
  message?: string;
}
export interface ContentTimerResult {
  realm: string;
  calls: Record<
    "undefined" | "null" | "object" | "htmlDDA",
    Record<ContentTimerMethod, ContentTimerCall>
  >;
  timerShapes: Record<ContentTimerMethod, {
    name: string;
    length: number;
    ownPrototype: boolean;
    constructible: boolean;
    constructError?: string;
  }>;
  callbackThisWindow?: boolean;
  callbackArgumentIdentity?: boolean;
  counts: {
    timeout: number;
    cancelledTimeout: number;
    interval: number;
    cancelledInterval: number;
  };
  callbackArgument: string | null;
  operationError?: { name: string; message: string };
}
export interface ContentTimerWindow extends TtsValidationWindow {
  wrappedJSObject: { __floorpRuntimeFixtureURL?: string };
}

export interface ContentTimerPageResult {
  pageTimersUnchanged: boolean;
  pageTimersNative: boolean;
  results: Record<string, ContentTimerResult>;
}
