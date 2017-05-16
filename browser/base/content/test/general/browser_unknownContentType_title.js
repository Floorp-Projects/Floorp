const url = "data:text/html;charset=utf-8,%3C%21DOCTYPE%20html%3E%3Chtml%3E%3Chead%3E%3Ctitle%3ETest%20Page%3C%2Ftitle%3E%3C%2Fhead%3E%3C%2Fhtml%3E";
const unknown_url = "http://example.com/browser/browser/base/content/test/general/unknownContentType_file.pif";

function waitForNewWindow() {
  return new Promise(resolve => {
    let listener = (win) => {
      Services.obs.removeObserver(listener, "toplevel-window-ready");
      win.addEventListener("load", () => {
        resolve(win);
      });
    };

    Services.obs.addObserver(listener, "toplevel-window-ready")
  });
}

add_task(async function() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
  let browser = tab.linkedBrowser;
  await promiseTabLoaded(gBrowser.selectedTab);

  is(gBrowser.contentTitle, "Test Page", "Should have the right title.")

  browser.loadURI(unknown_url);
  let win = await waitForNewWindow();
  is(win.location, "chrome://mozapps/content/downloads/unknownContentType.xul",
     "Should have seen the unknown content dialog.");
  is(gBrowser.contentTitle, "Test Page", "Should still have the right title.")

  win.close();
  await promiseWaitForFocus(window);
  gBrowser.removeCurrentTab();
});
