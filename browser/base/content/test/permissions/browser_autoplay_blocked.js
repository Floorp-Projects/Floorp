/*
 * Test that a blocked request to autoplay media is shown to the user
 */

const AUTOPLAY_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "browser_autoplay_blocked.html";

const AUTOPLAY_JS_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "browser_autoplay_js.html";

const SLOW_AUTOPLAY_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "browser_autoplay_blocked_slow.sjs";

const MUTED_AUTOPLAY_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "browser_autoplay_muted.html";

const EMPTY_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

const AUTOPLAY_PREF = "media.autoplay.default";
const AUTOPLAY_PERM = "autoplay-media";

function autoplayBlockedIcon() {
  return document.querySelector(
    "#blocked-permissions-container " +
      ".blocked-permission-icon.autoplay-media-icon"
  );
}

function permissionListBlockedIcons() {
  return document.querySelectorAll(
    "image.permission-popup-permission-icon.blocked-permission-icon"
  );
}

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function blockedIconShown() {
  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.is_visible(autoplayBlockedIcon());
  }, "Blocked icon is shown");
}

async function blockedIconHidden() {
  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.is_hidden(autoplayBlockedIcon());
  }, "Blocked icon is hidden");
}

function testPermListHasEntries(expectEntries) {
  let permissionsList = document.getElementById(
    "permission-popup-permission-list"
  );
  let listEntryCount = permissionsList.querySelectorAll(
    ".permission-popup-permission-item"
  ).length;
  if (expectEntries) {
    ok(listEntryCount, "List of permissions is not empty");
    return;
  }
  ok(!listEntryCount, "List of permissions is empty");
}

add_setup(async function () {
  registerCleanupFunction(() => {
    Services.perms.removeAll();
    Services.prefs.clearUserPref(AUTOPLAY_PREF);
  });
});

add_task(async function testMainViewVisible() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.ALLOWED);

  await BrowserTestUtils.withNewTab(AUTOPLAY_PAGE, async function () {
    ok(
      BrowserTestUtils.is_hidden(autoplayBlockedIcon()),
      "Blocked icon not shown"
    );

    await openPermissionPopup();
    testPermListHasEntries(false);
    await closePermissionPopup();
  });

  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  await BrowserTestUtils.withNewTab(AUTOPLAY_PAGE, async function (browser) {
    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );

    await blockedIconShown();

    await openPermissionPopup();
    testPermListHasEntries(true);

    let labelText = SitePermissions.getPermissionLabel(AUTOPLAY_PERM);
    let labels = permissionsList.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].textContent, labelText, "Correct value");

    let menulist = document.getElementById("permission-popup-menulist");
    Assert.equal(menulist.label, "Block Audio");

    await EventUtils.synthesizeMouseAtCenter(menulist, { type: "mousedown" });
    await TestUtils.waitForCondition(() => {
      return (
        menulist.getElementsByTagName("menuitem")[0].label ===
        "Allow Audio and Video"
      );
    });

    let menuitem = menulist.getElementsByTagName("menuitem")[0];
    Assert.equal(menuitem.getAttribute("label"), "Allow Audio and Video");

    menuitem.click();
    menulist.menupopup.hidePopup();
    await closePermissionPopup();

    let uri = Services.io.newURI(AUTOPLAY_PAGE);
    let state = PermissionTestUtils.getPermissionObject(
      uri,
      AUTOPLAY_PERM
    ).capability;
    Assert.equal(state, Services.perms.ALLOW_ACTION);
  });

  Services.perms.removeAll();
});

add_task(async function testGloballyBlockedOnNewWindow() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      AUTOPLAY_PAGE
    );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    AUTOPLAY_PAGE
  );
  await blockedIconShown();

  Assert.deepEqual(
    SitePermissions.getForPrincipal(
      principal,
      AUTOPLAY_PERM,
      tab.linkedBrowser
    ),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_PERSISTENT,
    }
  );

  let promiseWin = BrowserTestUtils.waitForNewWindow();
  gBrowser.replaceTabWithWindow(tab);
  let win = await promiseWin;
  tab = win.gBrowser.selectedTab;

  Assert.deepEqual(
    SitePermissions.getForPrincipal(
      principal,
      AUTOPLAY_PERM,
      tab.linkedBrowser
    ),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_PERSISTENT,
    }
  );

  SitePermissions.removeFromPrincipal(
    principal,
    AUTOPLAY_PERM,
    tab.linkedBrowser
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testBFCache() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  await BrowserTestUtils.withNewTab("about:home", async function (browser) {
    BrowserTestUtils.startLoadingURIString(browser, AUTOPLAY_PAGE);
    await blockedIconShown();

    gBrowser.goBack();
    await blockedIconHidden();

    // Not sure why using `gBrowser.goForward()` doesn't trigger document's
    // visibility changes in some debug build on try server, which makes us not
    // to receive the blocked event.
    await SpecialPowers.spawn(browser, [], () => {
      content.history.forward();
    });
    await blockedIconShown();
  });

  Services.perms.removeAll();
});

add_task(async function testBlockedIconFromCORSIframe() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  await BrowserTestUtils.withNewTab(EMPTY_PAGE, async browser => {
    const blockedIconShownPromise = blockedIconShown();
    const CORS_AUTOPLAY_PAGE = AUTOPLAY_PAGE.replace(
      "example.com",
      "example.org"
    );
    info(`Load CORS autoplay on an iframe`);
    await SpecialPowers.spawn(browser, [CORS_AUTOPLAY_PAGE], async url => {
      const iframe = content.document.createElement("iframe");
      iframe.src = url;
      content.document.body.appendChild(iframe);
      info("Wait until iframe finishes loading");
      await new Promise(r => (iframe.onload = r));
    });
    await blockedIconShownPromise;
    ok(true, "Blocked icon shown for the CORS autoplay iframe");
  });

  Services.perms.removeAll();
});

add_task(async function testChangingBlockingSettingDuringNavigation() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  await BrowserTestUtils.withNewTab("about:home", async function (browser) {
    await blockedIconHidden();
    BrowserTestUtils.startLoadingURIString(browser, AUTOPLAY_PAGE);
    await blockedIconShown();
    Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.ALLOWED);

    gBrowser.goBack();
    await blockedIconHidden();

    gBrowser.goForward();

    // Sleep here to prevent false positives, the icon gets shown with an
    // async `GloballyAutoplayBlocked` event. The sleep gives it a little
    // time for it to show otherwise there is a chance it passes before it
    // would have shown.
    await sleep(100);
    ok(
      BrowserTestUtils.is_hidden(autoplayBlockedIcon()),
      "Blocked icon is hidden"
    );
  });

  Services.perms.removeAll();
});

add_task(async function testSlowLoadingPage() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:home"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SLOW_AUTOPLAY_PAGE
  );
  await blockedIconShown();

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  // Wait until the blocked icon is hidden by switching tabs
  await blockedIconHidden();
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await blockedIconShown();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  Services.perms.removeAll();
});

add_task(async function testBlockedAll() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED_ALL);

  await BrowserTestUtils.withNewTab("about:home", async function (browser) {
    await blockedIconHidden();
    BrowserTestUtils.startLoadingURIString(browser, MUTED_AUTOPLAY_PAGE);
    await blockedIconShown();

    await openPermissionPopup();

    Assert.equal(
      permissionListBlockedIcons().length,
      1,
      "Blocked icon is shown"
    );

    let menulist = document.getElementById("permission-popup-menulist");
    await EventUtils.synthesizeMouseAtCenter(menulist, { type: "mousedown" });
    await TestUtils.waitForCondition(() => {
      return (
        menulist.getElementsByTagName("menuitem")[1].label === "Block Audio"
      );
    });

    let menuitem = menulist.getElementsByTagName("menuitem")[0];
    menuitem.click();
    menulist.menupopup.hidePopup();
    await closePermissionPopup();
    gBrowser.reload();
    await blockedIconHidden();
  });
  Services.perms.removeAll();
});

add_task(async function testMultiplePlayNotificationsFromJS() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  await BrowserTestUtils.withNewTab("about:home", async function (browser) {
    let count = 0;
    browser.addEventListener("GloballyAutoplayBlocked", function () {
      is(++count, 1, "Shouldn't get more than one autoplay blocked event");
    });

    await blockedIconHidden();

    BrowserTestUtils.startLoadingURIString(browser, AUTOPLAY_JS_PAGE);

    await blockedIconShown();

    // Sleep here a bit to ensure that multiple events don't arrive.
    await sleep(100);

    is(count, 1, "Shouldn't have got more events");
  });

  Services.perms.removeAll();
});
