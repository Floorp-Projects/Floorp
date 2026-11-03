// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export interface DesktopCaptureBrowser {
  contentTitle: string;
  browsingContext: { browserId: number };
  messageManager: {
    loadFrameScript(source: string, allFrames: boolean): void;
  };
  fixupAndLoadURIString(
    source: string,
    options: { triggeringPrincipal: nsIPrincipal },
  ): void;
}

export interface DesktopCaptureTab {
  linkedBrowser: DesktopCaptureBrowser;
  isConnected: boolean;
}

export interface DesktopCaptureWindow extends Window {
  gBrowser: {
    selectedTab: DesktopCaptureTab;
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): DesktopCaptureTab;
    removeTab(tab: DesktopCaptureTab): void;
  };
  navigator: Navigator & {
    mediaDevices: MediaDevices & { mozFloorpCapturePickerVersion?: number };
  };
  RTCPeerConnection: typeof RTCPeerConnection;
  MediaStream: typeof MediaStream;
  AudioContext: typeof AudioContext;
  MediaRecorder: typeof MediaRecorder;
}

export interface DesktopCaptureContext {
  viewType: string;
  xulBrowser: DesktopCaptureBrowser;
}

export interface DesktopCaptureExtension {
  baseURI: nsIURI;
  backgroundContext?: DesktopCaptureContext;
  views: Set<DesktopCaptureContext>;
  terminateBackground(): Promise<void>;
  wakeupBackground(): Promise<void>;
  apiManager: {
    asyncGetAPI(
      name: "runtime",
      extension: DesktopCaptureExtension,
      scope: "addon_parent",
    ): Promise<{
      getAPI(context: DesktopCaptureContext): {
        runtime: {
          getContexts(filter: Record<string, never>): Promise<
            { documentUrl?: string }[]
          >;
        };
      };
    }>;
  };
}

export interface DesktopCaptureExtensionParent {
  GlobalManager: { extensionMap: Map<string, DesktopCaptureExtension> };
}

export interface DesktopCaptureAddon {
  uninstall(): Promise<void>;
}

export interface DesktopCaptureAddonManager {
  installTemporaryAddon(file: nsIFile): Promise<DesktopCaptureAddon>;
}

export interface DesktopCaptureDialogWindow extends Window {
  closed: boolean;
}

export interface DesktopCaptureWindowMediator {
  getEnumerator(
    windowType: string | null,
  ): Iterable<DesktopCaptureDialogWindow>;
}

export interface DesktopCaptureDialogList {
  itemCount: number;
  selectedIndex: number;
  getItemAtIndex(index: number): { label: string };
}

export interface DesktopCaptureDialog {
  acceptDialog(): void;
  cancelDialog(): void;
}

export interface DesktopCaptureCommand {
  start?: boolean;
  cancel?: boolean;
  inspect?: boolean;
  audio?: boolean;
  record?: boolean;
  hold?: boolean;
}

export interface DesktopSourcePage {
  unloaded: boolean;
  windowGlobal: { documentPrincipal: nsIPrincipal } | null;
  onClose: (() => void) | null;
  open(): Promise<DesktopSourcePage>;
  select(
    choice:
      | { kind: "screen" | "window"; pickerToken: string }
      | { kind: "tab"; browserId: number; audioTab: DesktopCaptureTab },
  ): Promise<{ video: boolean; audio: boolean }>;
  offer(sessionId: string): Promise<RTCSessionDescriptionInit>;
  answer(sessionId: string, answer: RTCSessionDescriptionInit): Promise<void>;
  close(): void;
}

export interface DesktopSourceModule {
  DesktopSourcePage: new (
    extension: DesktopCaptureExtension,
  ) => DesktopSourcePage;
}

// The harness implementation accepts omitted diagnostic text at runtime.
export type DesktopCaptureAssertEquals = <T>(
  actual: T,
  expected: T,
  message?: string,
) => void;
