// SPDX-License-Identifier: MPL-2.0
/** A document identity, not just a URL: navigation can replace a WindowGlobal. */
type WebBrowser = {
  isConnected?: boolean;
  currentURI?: { scheme: string; spec: string };
  browsingContext?: { currentWindowGlobal?: { isClosed?: boolean } | null };
};
export function captureWebDocument(input: unknown) {
  const browser = input as WebBrowser | null;
  try {
    const uri = browser?.currentURI;
    const global = browser?.browsingContext?.currentWindowGlobal;
    if (
      !browser || browser.isConnected === false || !uri ||
      !["http", "https"].includes(uri.scheme) || !global || global.isClosed
    ) return null;
    const url = uri.spec;
    return {
      current: () => {
        try {
          return browser.isConnected !== false && !global.isClosed &&
            browser.currentURI?.spec === url &&
            browser.browsingContext?.currentWindowGlobal === global;
        } catch {
          return false;
        }
      },
    };
  } catch {
    return null;
  }
}

/** Background observations never own installation or leak rejected promises. */
export class LatestWebObservation {
  private generation = 0;
  async run(
    browser: unknown,
    selected: () => unknown,
    operation: (current: () => boolean) => Promise<void>,
    report: (error: unknown) => void,
  ): Promise<void> {
    const generation = ++this.generation;
    const document = captureWebDocument(browser);
    if (!document) return;
    const current = () => {
      try {
        return generation === this.generation && selected() === browser &&
          document.current();
      } catch {
        return false;
      }
    };
    try {
      if (current()) await operation(current);
    } catch (error) {
      // Expected document destruction/navigation is a cancellation, not an error.
      if (current()) report(error);
    }
  }
}
