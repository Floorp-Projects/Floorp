// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

import type { FrozenCorpus } from "../../../../../../tools/chrome-extension-runtime/corpus-types.ts";
import type {
  FrozenCorpusScriptEvidence,
  NativeCorpusBackgroundEvidence,
} from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
import type { ChromeConversionReport } from "../../types.ts";
import type { ActionObservation } from "../../../../../../tools/chrome-extension-runtime/browser-tools-test-types.ts";

export type CaptureCorpusTruthy<Value> = Exclude<
  Value,
  false | 0 | "" | null | undefined
>;

export type CaptureCorpusInspectionSource<Result> =
  | string
  | (() => Result | Promise<Result>);

export interface CaptureCorpusReply<Result> {
  data: { error?: string; value?: Result };
}

export interface CaptureCorpusBrowser {
  contentTitle: string;
  currentURI: nsIURI;
  messageManager: {
    addMessageListener<Result>(
      name: string,
      listener: (message: CaptureCorpusReply<Result>) => void,
    ): void;
    removeMessageListener<Result>(
      name: string,
      listener: (message: CaptureCorpusReply<Result>) => void,
    ): void;
    loadFrameScript(source: string, allFrames: boolean): void;
  };
}

export interface CaptureCorpusTab {
  linkedBrowser: CaptureCorpusBrowser;
  isConnected: boolean;
}

export interface CaptureCorpusPermissionPanel extends XULElement {
  button?: HTMLButtonElement;
}

export interface CaptureCorpusWindow extends Window {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    tabs: CaptureCorpusTab[];
    selectedTab: CaptureCorpusTab;
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): CaptureCorpusTab;
    removeTab(tab: CaptureCorpusTab): void;
  };
  PopupNotifications: {
    getNotification(name: string): object | null;
    panel: XULElement & { state: string };
  };
}

export interface CaptureCorpusView {
  viewType: string;
  xulBrowser: CaptureCorpusBrowser | null;
  uri?: nsIURI;
}

export interface CaptureCorpusExtension {
  backgroundContext?: CaptureCorpusView;
  views: Set<CaptureCorpusView>;
  tabManager: { getWrapper(tab: CaptureCorpusTab): { id: number } };
  apiManager: {
    asyncGetAPI(
      name: string,
      extension: CaptureCorpusExtension,
      scope: string,
    ): Promise<{ triggerAction(window: CaptureCorpusWindow): Promise<void> }>;
  };
}

export interface CaptureCorpusExtensionParentModule {
  ExtensionParent: {
    GlobalManager: { extensionMap: Map<string, CaptureCorpusExtension> };
  };
}

export interface CaptureCorpusAddonManagerModule {
  AddonManager: {
    installTemporaryAddon(file: nsIFile): Promise<CaptureCorpusAddon>;
  };
}

export interface CaptureCorpusAddon {
  uninstall(): Promise<void>;
}

export type CaptureCorpusFrozenLock = FrozenCorpus;

export interface CaptureCorpusAccountEvidence {
  text?: string;
  title: string;
  url: string;
}

export interface CaptureCorpusImage {
  width: number;
  height: number;
  src: string;
  pixel: number[];
  sha256?: string;
}

export interface CaptureCorpusCanvas {
  width: number;
  height: number;
  pixel: number[];
}

export interface CaptureCorpusMedia extends CaptureCorpusImage {
  path: string;
  bytes: number;
  sha256: string;
}

export interface CaptureCorpusViewSnapshot {
  type: string;
  title?: string;
  url?: string;
}

export interface CaptureCorpusDownload {
  source: { url: string };
  target: { path: string };
  succeeded: boolean;
  error?: { message: string } | null;
}

export interface CaptureCorpusDownloadsModule {
  Downloads: {
    ALL: string;
    getList(type: string): Promise<{
      getAll(): Promise<CaptureCorpusDownload[]>;
    }>;
  };
}

export interface CaptureCorpusReport {
  output?: string;
  stage: "actual-package-primary-operation";
  nativeQualified: boolean;
  cleanupCompleted: boolean;
  cleanupError?: string;
  sourceIntegrity?: FrozenCorpusScriptEvidence;
  nativeEvidence?: NativeCorpusBackgroundEvidence;
  conversion?: ChromeConversionReport;
  id: string;
  version?: string;
  packageSha256: string;
  primaryOperationResult: "not-run" | "blocked-account" | "pass" | "failure";
  runtimeVersion: string;
  runtimeBuildId: string;
  views: CaptureCorpusViewSnapshot[];
  console: string[];
  conversionWarnings?: string[];
  source?: { url: string; id: number };
  accountEvidence?: CaptureCorpusAccountEvidence;
  nativeAction?: ActionObservation;
  image?: CaptureCorpusImage;
  savedImage?: CaptureCorpusCanvas;
  canvas?: CaptureCorpusCanvas;
  media?: CaptureCorpusMedia;
  downloadPermissionPrompt?: string | null;
  error?: string;
  browserPages?: { url: string; title: string }[];
  downloads?: {
    url: string;
    path: string;
    succeeded: boolean;
    error?: string;
  }[];
  finalViews?: {
    type: string;
    url?: string;
    snapshot:
      | string
      | { text?: string; ready: string; location: string };
  }[];
}
