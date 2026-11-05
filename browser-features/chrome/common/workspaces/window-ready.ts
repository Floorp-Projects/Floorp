// SPDX-License-Identifier: MPL-2.0

interface WorkspaceWindow extends Window {
  gBrowser?: { tabContainer?: unknown; selectedTab?: unknown };
  gBrowserInit?: { delayedStartupFinished?: boolean };
}

interface StartupObservers {
  addObserver(observer: nsIObserver, topic: string): void;
  removeObserver(observer: nsIObserver, topic: string): void;
}

/** SessionStore readiness is shared; gBrowser readiness belongs to each window. */
export function waitForWorkspaceWindow(
  win: WorkspaceWindow,
  observers: StartupObservers,
  signal: AbortSignal,
): Promise<boolean> {
  const ready = () =>
    win.document.readyState !== "loading" &&
    !!win.document.getElementById("mainPopupSet") &&
    !!win.gBrowser?.tabContainer && !!win.gBrowser.selectedTab;
  if (signal.aborted || win.closed) return Promise.resolve(false);
  if (ready()) return Promise.resolve(true);
  if (win.gBrowserInit?.delayedStartupFinished) return Promise.resolve(false);
  return new Promise((resolve) => {
    let listening = false;
    const finish = (value: boolean) => {
      if (listening) {
        listening = false;
        observers.removeObserver(observer, "browser-delayed-startup-finished");
      }
      signal.removeEventListener("abort", onAbort);
      resolve(value);
    };
    const observer: nsIObserver = {
      observe(subject) {
        if ((subject as unknown) !== win) return;
        finish(!signal.aborted && !win.closed && ready());
      },
    };
    const onAbort = () => finish(false);
    observers.addObserver(observer, "browser-delayed-startup-finished");
    listening = true;
    signal.addEventListener("abort", onAbort, { once: true });
    if (signal.aborted || win.closed) finish(false);
    else if (ready()) finish(true);
    else if (win.gBrowserInit?.delayedStartupFinished) finish(false);
  });
}
