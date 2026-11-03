// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export type DesktopSourceKind = "screen" | "window";

export interface CaptureBrowser {
  contentTitle: string;
  browsingContext: {
    browserId: number;
    currentWindowGlobal: {
      osPid: number;
      documentURI?: nsIURI;
    } | null;
    children: {
      currentWindowGlobal: {
        osPid: number;
        documentURI?: nsIURI;
      } | null;
    }[];
  };
  messageManager: {
    loadFrameScript(source: string, allFrames: boolean): void;
    addMessageListener(
      name: string,
      listener: (message: { data: unknown }) => void,
    ): void;
    removeMessageListener(
      name: string,
      listener: (message: { data: unknown }) => void,
    ): void;
  };
  fixupAndLoadURIString(
    source: string,
    options: { triggeringPrincipal: nsIPrincipal },
  ): void;
}

export interface CaptureTab {
  linkedBrowser: CaptureBrowser;
  isConnected: boolean;
}

export interface CaptureNativeWindow extends Window {
  gBrowser: {
    selectedTab: CaptureTab;
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): CaptureTab;
    removeTab(tab: CaptureTab): void;
  };
}

export interface DesktopNativeWindow extends CaptureNativeWindow {
  navigator: Navigator & {
    mediaDevices: MediaDevices & { mozFloorpCapturePickerVersion: number };
  };
  PopupNotifications: {
    panel: XULElement & { state: string };
    getNotification(name: string, browser: CaptureBrowser): object | null;
  };
}

export interface CaptureDialogWindow extends Window {
  closed: boolean;
}

export interface NativeDialogList extends XULElement {
  itemCount: number;
  selectedIndex: number;
}

export interface NativeDialog extends XULElement {
  acceptDialog(): void;
  cancelDialog(): void;
}

export interface DesktopSourceChoice extends XULElement {
  label: string;
  mediaSource: DesktopSourceKind;
}

export interface DesktopSourceList extends XULElement {
  selectedItem: DesktopSourceChoice;
}

export interface CapturePermissionPanel extends XULElement {
  button: HTMLButtonElement;
  secondaryButton: HTMLButtonElement;
}

export interface CaptureAddon {
  uninstall(): Promise<void>;
}

export interface CaptureAddonManager {
  installTemporaryAddon(file: nsIFile): Promise<CaptureAddon>;
}

export interface CaptureView {
  viewType: string;
  xulBrowser: CaptureBrowser;
}

export interface DesktopExtension {
  principal: nsIPrincipal;
  backgroundContext?: CaptureView;
  views: Set<CaptureView>;
}

export interface DesktopExtensionParent {
  GlobalManager: { extensionMap: Map<string, DesktopExtension> };
}

export interface AudioExtension extends DesktopExtension {
  terminateBackground(): Promise<void>;
  apiManager: {
    asyncGetAPI<Name extends keyof AudioNativeModules>(
      name: Name,
      extension: AudioExtension,
      scope: string,
    ): Promise<AudioNativeModules[Name]>;
  };
}

export interface AudioExtensionParent {
  GlobalManager: { extensionMap: Map<string, AudioExtension> };
}

export interface AudioNativeModules {
  browserAction: { action: { triggerClickOrPopup(tab: CaptureTab): void } };
  tabCapture: {
    getAPI(view: CaptureView): {
      tabCapture: { getCapturedTabs(): Promise<object[]> };
    };
  };
  offscreen: {
    getAPI(view: CaptureView): {
      offscreen: { closeDocument(): Promise<void> };
    };
  };
}

export interface DesktopResponseIdentity {
  commandId: string;
  requestId: number;
  phase: string;
  error?: string;
}

export interface DesktopSelectionStatus extends DesktopResponseIdentity {
  phase: "requested" | "selected" | "denied" | "error";
  options?: { canRequestAudioTrack: boolean };
}

export interface DesktopRecording extends DesktopResponseIdentity {
  kind: DesktopSourceKind;
  liveGreen: number;
  replayGreen: number;
  width: number;
  height: number;
  bytes: number;
  mimeType: string;
  audio: number;
  base64: string;
}

export type DesktopScreenCase =
  | "block-denial"
  | "kind-cancel"
  | "record-screen"
  | "record-window";

export interface DesktopScreenPermission {
  state: number;
  scope: string;
}

export interface DesktopSitePermissions {
  UNKNOWN: number;
  BLOCK: number;
  SCOPE_TEMPORARY: string;
  getForPrincipal(
    principal: nsIPrincipal,
    permission: string,
    browser: CaptureBrowser,
  ): DesktopScreenPermission;
}

export interface DesktopScreenReport {
  os: string;
  case: DesktopScreenCase;
  initialScreenPermission?: DesktopScreenPermission;
  requests: DesktopSelectionStatus[];
  denial?: {
    commandId: string;
    requestId: number;
    callbackEmpty: boolean;
    permission: DesktopScreenPermission;
    repeatedRequestDenied: boolean;
  };
  kindCancellation?: {
    commandId: string;
    requestId: number;
    callbackEmpty: boolean;
    permission: DesktopScreenPermission;
    freshPermissionPrompt: boolean;
  };
  selections: {
    kind: DesktopSourceKind;
    label: string;
    sources: { label: string; mediaSource: DesktopSourceKind }[];
  }[];
  recordings: (Omit<DesktopRecording, "base64"> & {
    path: string;
    sha256: string;
  })[];
}

export type AudioRecordingReply =
  | { bytes: ArrayBuffer; mimeType: string }
  | { error: string };

export interface AudioRecordingArtifact {
  path: string;
  mimeType: string;
  bytes: number;
  sha256: string;
}

export type AudioRecordingVariant = "tone" | "frame" | "navigation";

export interface AudioRecordingResult {
  phase: "done";
  rms: number;
  peak: number;
  frameLevel: number;
  bytes: number;
  audioTracks: number;
  videoTracks: number;
  endedBeforeManualStop: boolean;
}
