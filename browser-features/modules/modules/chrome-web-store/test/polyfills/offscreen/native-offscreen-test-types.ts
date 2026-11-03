// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
export interface NativeOffscreenRootPresence {
  api: "browser" | "chrome";
  keys: string[];
  entries: {
    name: string;
    inRoot: boolean;
    own: boolean;
    descriptor: boolean;
  }[];
}
export interface NativeOffscreenObservation {
  namespaces: NativeOffscreenRootPresence[];
  storage: string;
  runtime: string;
  background: string;
  chromeStorage: string;
  chromeRuntime: string;
  chromeBackground: string;
}
