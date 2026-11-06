// SPDX-License-Identifier: MPL-2.0

interface BrowserWindow extends Window {
  gBrowser?: { tabs?: unknown; addTabsProgressListener?: unknown };
  gBrowserInit?: { delayedStartupFinished?: boolean };
}
interface StartupObservers {
  addObserver(observer: nsIObserver, topic: string): void;
  removeObserver(observer: nsIObserver, topic: string): void;
}

/** Start only when this window owns a usable browser; disposal cancels startup. */
export function startWhenBrowserReady(
  win: BrowserWindow,
  observers: StartupObservers,
  start: () => void,
): () => void {
  let stopped = false;
  let listening = false;
  const ready = () =>
    !!win.gBrowser?.tabs &&
    typeof win.gBrowser.addTabsProgressListener === "function";
  const remove = () => {
    if (listening) {
      listening = false;
      observers.removeObserver(observer, "browser-delayed-startup-finished");
    }
  };
  const tryStart = () => {
    if (stopped || win.closed || !ready()) return;
    stopped = true;
    remove();
    start();
  };
  const observer: nsIObserver = {
    observe(subject) {
      if ((subject as unknown) === win) {
        tryStart();
        if (!stopped && win.gBrowserInit?.delayedStartupFinished) {
          remove();
          console.error(
            "[TabSleepExclusion] Browser unavailable after window startup",
          );
        }
      }
    },
  };
  if (!win.closed && ready()) tryStart();
  else if (!win.closed && !win.gBrowserInit?.delayedStartupFinished) {
    observers.addObserver(observer, "browser-delayed-startup-finished");
    listening = true;
    tryStart();
  }
  return () => {
    stopped = true;
    remove();
  };
}
