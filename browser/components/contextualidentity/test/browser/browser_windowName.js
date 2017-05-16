/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


const USER_CONTEXTS = [
  "default",
  "personal",
  "work",
];

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "contextualidentity/test/browser/empty_file.html";

add_task(async function setup() {
  // make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
    ["browser.link.open_newwindow", 3],
  ]});
});

add_task(async function test() {
  info("Creating first tab...");
  let tab1 = BrowserTestUtils.addTab(gBrowser, BASE_URI + "?old", {userContextId: 1});
  let browser1 = gBrowser.getBrowserForTab(tab1);
  await BrowserTestUtils.browserLoaded(browser1);
  await ContentTask.spawn(browser1, null, function(opts) {
    content.window.name = "tab-1";
  });

  info("Creating second tab...");
  let tab2 = BrowserTestUtils.addTab(gBrowser, BASE_URI + "?old", {userContextId: 2});
  let browser2 = gBrowser.getBrowserForTab(tab2);
  await BrowserTestUtils.browserLoaded(browser2);
  await ContentTask.spawn(browser2, null, function(opts) {
    content.window.name = "tab-2";
  });

  // Let's try to open a window from tab1 with a name 'tab-2'.
  info("Opening a window from the first tab...");
  await ContentTask.spawn(browser1, { url: BASE_URI + "?new" }, async function(opts) {
    await (new content.window.wrappedJSObject.Promise(resolve => {
      let w = content.window.wrappedJSObject.open(opts.url, "tab-2");
      w.onload = function() { resolve(); }
    }));
  });

  is(browser1.contentTitle, "?old", "Tab1 title must be 'old'");
  is(browser1.contentPrincipal.userContextId, 1, "Tab1 UCI must be 1");

  is(browser2.contentTitle, "?old", "Tab2 title must be 'old'");
  is(browser2.contentPrincipal.userContextId, 2, "Tab2 UCI must be 2");

  let found = false;
  for (let i = 0; i < gBrowser.tabContainer.childNodes.length; ++i) {
    let tab = gBrowser.tabContainer.childNodes[i];
    let browser = gBrowser.getBrowserForTab(tab);
    if (browser.contentTitle == "?new") {
      is(browser.contentPrincipal.userContextId, 1, "Tab3 UCI must be 1");
      isnot(browser, browser1, "Tab3 is not browser 1");
      isnot(browser, browser2, "Tab3 is not browser 2");
      gBrowser.removeTab(tab);
      found = true;
      break;
    }
  }

  ok(found, "We have tab3");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});
