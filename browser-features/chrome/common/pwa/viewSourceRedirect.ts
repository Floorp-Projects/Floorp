import type { ViewSourceArgs, ViewSourceWindow } from "./type.ts";

const { BrowserWindowTracker } = ChromeUtils.importESModule(
  "resource:///modules/BrowserWindowTracker.sys.mjs",
);

async function getBrowserWindow(): Promise<ViewSourceWindow> {
  return (BrowserWindowTracker.getTopWindow() ??
    await BrowserWindowTracker.promiseOpenWindow()) as ViewSourceWindow;
}

function opensInTab(): boolean {
  return Services.prefs.getBoolPref("view_source.tab", true);
}

function openSourceBrowser(target: ViewSourceWindow): XULBrowserElement {
  const inNewWindow = !opensInTab();
  const tabBrowser = target.gBrowser;
  const tab = tabBrowser.addTab("about:blank", {
    inBackground: inNewWindow,
    skipAnimation: inNewWindow,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  const browser = tabBrowser.getBrowserForTab(tab);
  if (inNewWindow) {
    tabBrowser.hideTab(tab);
    tabBrowser.replaceTabsWithWindow(tab);
  } else {
    target.focus();
  }
  return browser;
}

export function redirectViewSourceToBrowserWindow(
  win: ViewSourceWindow,
): void {
  const viewSourceUtils = win.gViewSourceUtils;
  if (viewSourceUtils.__floorpSsbPatched) {
    return;
  }
  viewSourceUtils.__floorpSsbPatched = true;

  win.BrowserCommands.viewSourceOfDocument = async (args: ViewSourceArgs) => {
    const target = await getBrowserWindow();
    await target.BrowserCommands.viewSourceOfDocument(args);
    if (
      opensInTab() &&
      !Services.prefs.getBoolPref("view_source.editor.external", false)
    ) {
      target.focus();
    }
  };

  const viewPartialSource = viewSourceUtils.viewPartialSourceInBrowser.bind(
    viewSourceUtils,
  );
  viewSourceUtils.viewPartialSourceInBrowser = (browsingContext) =>
    viewPartialSource(
      browsingContext,
      async () => openSourceBrowser(await getBrowserWindow()),
    );
}
