/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


const USER_CONTEXTS = [
  "default",
  "personal",
  "work",
];

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "contextualidentity/test/browser/empty_file.html";

add_task(function* setup() {
  // make sure userContext is enabled.
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true],
      ["browser.link.open_newwindow", 3],
    ]}, resolve);
  });
});

add_task(function* test() {
  info("Creating first tab...");
  let tab1 = gBrowser.addTab(BASE_URI + '?old', {userContextId: 1});
  let browser1 = gBrowser.getBrowserForTab(tab1);
  yield BrowserTestUtils.browserLoaded(browser1);
  yield ContentTask.spawn(browser1, null, function(opts) {
    content.window.name = 'tab-1';
  });

  info("Creating second tab...");
  let tab2 = gBrowser.addTab(BASE_URI + '?old', {userContextId: 2});
  let browser2 = gBrowser.getBrowserForTab(tab2);
  yield BrowserTestUtils.browserLoaded(browser2);
  yield ContentTask.spawn(browser2, null, function(opts) {
    content.window.name = 'tab-2';
  });

  // Let's try to open a window from tab1 with a name 'tab-2'.
  info("Opening a window from the first tab...");
  yield ContentTask.spawn(browser1, { url: BASE_URI + '?new' }, function(opts) {
    yield (new content.window.wrappedJSObject.Promise(resolve => {
      let w = content.window.wrappedJSObject.open(opts.url, 'tab-2');
      w.onload = function() { resolve(); }
    }));
  });

  is(browser1.contentDocument.title, '?old', "Tab1 title must be 'old'");
  is(browser1.contentDocument.nodePrincipal.userContextId, 1, "Tab1 UCI must be 1");

  is(browser2.contentDocument.title, '?old', "Tab2 title must be 'old'");
  is(browser2.contentDocument.nodePrincipal.userContextId, 2, "Tab2 UCI must be 2");

  let found = false;
  for (let i = 0; i < gBrowser.tabContainer.childNodes.length; ++i) {
    let tab = gBrowser.tabContainer.childNodes[i];
    let browser = gBrowser.getBrowserForTab(tab);
    if (browser.contentDocument.title == '?new') {
      is(browser.contentDocument.nodePrincipal.userContextId, 1, "Tab3 UCI must be 1");
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
