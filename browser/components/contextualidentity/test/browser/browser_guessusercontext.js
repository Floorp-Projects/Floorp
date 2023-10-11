/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DEFAULT = Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
const PERSONAL = 1;
const WORK = 2;
const HOST_MOCHI = makeURI(
  "http://mochi.test:8888/browser/browser/components/contextualidentity/test/browser/blank.html"
);
const HOST_EXAMPLE = makeURI(
  "https://example.com/browser/browser/components/contextualidentity/test/browser/blank.html"
);

const {
  URILoadingHelper: { guessUserContextId },
} = ChromeUtils.importESModule("resource:///modules/URILoadingHelper.sys.mjs");

async function openTabInUserContext(uri, userContextId, win = window) {
  let { gBrowser } = win;
  let tab = BrowserTestUtils.addTab(gBrowser, uri, { userContextId });
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();
  await BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab));
  return tab;
}

registerCleanupFunction(async function cleanup() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeTab(gBrowser.selectedTab, { animate: false });
  }
});

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });
});

add_task(async function test() {
  is(guessUserContextId(null), null, "invalid uri - null");
  is(guessUserContextId(HOST_EXAMPLE), null, "no tabs - null");
  await openTabInUserContext(HOST_EXAMPLE.spec, PERSONAL);
  is(guessUserContextId(HOST_EXAMPLE), PERSONAL, "one tab - matches container");
  is(guessUserContextId(HOST_MOCHI), null, "one tab - doesn't match container");

  await openTabInUserContext(HOST_MOCHI.spec, PERSONAL);
  is(guessUserContextId(HOST_MOCHI), PERSONAL, "one tab - matches container");
  await openTabInUserContext(HOST_MOCHI.spec);
  await openTabInUserContext(HOST_MOCHI.spec);
  is(
    guessUserContextId(HOST_MOCHI),
    DEFAULT,
    "can guess guess default container"
  );

  await openTabInUserContext(HOST_EXAMPLE.spec, WORK);
  is(guessUserContextId(HOST_EXAMPLE), PERSONAL, "same number - use first");
  await openTabInUserContext(HOST_EXAMPLE.spec, WORK);
  is(guessUserContextId(HOST_EXAMPLE), WORK, "multiple per host - max");

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await openTabInUserContext(HOST_EXAMPLE.spec, PERSONAL, win);
  await openTabInUserContext(HOST_EXAMPLE.spec, PERSONAL, win);
  is(guessUserContextId(HOST_EXAMPLE), PERSONAL, "count across windows");

  await BrowserTestUtils.closeWindow(win);
  is(guessUserContextId(HOST_EXAMPLE), WORK, "forgets closed window");

  // Check the opener flow more directly
  let browsingContext = window.browserDOMWindow.openURI(
    makeURI(HOST_EXAMPLE.spec + "?new"),
    null,
    Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
    Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  await BrowserTestUtils.browserLoaded(browsingContext.embedderElement);
  is(
    browsingContext.embedderElement,
    gBrowser.selectedBrowser,
    "opener selected"
  );
  is(
    gBrowser.selectedTab.getAttribute("usercontextid"),
    WORK.toString(),
    "opener flow"
  );
  is(guessUserContextId(HOST_EXAMPLE), WORK, "still the most common");
  is(
    guessUserContextId(HOST_MOCHI),
    DEFAULT,
    "still matches default container"
  );
});
