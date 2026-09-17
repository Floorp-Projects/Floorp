export type PrefValue = string | number | boolean;
export type UiTestResult = { name: string; ok: boolean; error?: string };
export type UiTestReport = {
  status: "running" | "done";
  results: UiTestResult[];
};
export type BridgeCall = { method: string; args: unknown[] };
export interface UiTestBrowser extends Element {
  focus: () => void;
  loadURI: (uri: unknown, options: { triggeringPrincipal: unknown }) => void;
  messageManager: {
    loadFrameScript: (url: string, delayed: boolean) => void;
    addMessageListener: (
      name: string,
      listener: (message: { data: string | undefined }) => void,
    ) => void;
    removeMessageListener: (
      name: string,
      listener: (message: { data: string | undefined }) => void,
    ) => void;
    sendAsyncMessage: (name: string) => void;
  };
}
export interface UiTestChromeHost {
  document: Document & { createXULElement: (name: string) => UiTestBrowser };
  Services: {
    io: { newURI: (url: string) => unknown };
    scriptSecurityManager: { getSystemPrincipal: () => unknown };
  };
}
