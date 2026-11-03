// SPDX-License-Identifier: MPL-2.0
export interface WindowUpdateSnapshot {
  id: number;
  left: number;
  top: number;
  width: number;
  height: number;
  state: string;
}
export interface WindowUpdateReport {
  phase: string;
  targetId?: number;
  requested?: Partial<WindowUpdateSnapshot>;
  returned?: WindowUpdateSnapshot;
  immediate?: WindowUpdateSnapshot;
  before?: WindowUpdateSnapshot;
  queuedFirst?: WindowUpdateSnapshot;
  elapsed?: number;
  error?: string;
  checks?: string[];
}
export interface WindowUpdateView {
  viewType: string;
  xulBrowser?: {
    contentTitle: string;
    messageManager: {
      loadFrameScript(source: string, allFrames: boolean): void;
    };
  };
}
export type WindowUpdateNativeWindow = Window & { windowRoot: EventTarget };
export interface WindowUpdateExtension {
  views: Set<WindowUpdateView>;
  apiManager: {
    global: {
      windowTracker: {
        getWindow(id: number): WindowUpdateNativeWindow;
      };
    };
  };
}
export interface WindowUpdateAddon {
  uninstall(): Promise<void>;
}
export interface WindowUpdateNativeEvent {
  type: string;
  at: number;
  width: number;
  height: number;
  left: number;
  top: number;
}
