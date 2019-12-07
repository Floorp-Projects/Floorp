/**
 * In this test, we check that the content can't open more than one popup at a
 * time (depending on "dom.allow_mulitple_popups" preference value).
 */

const TEST_DOMAIN = "http://example.net";
const TEST_PATH = "/browser/dom/base/test/";
const CHROME_DOMAIN = "chrome://mochitests/content";

requestLongerTimeout(2);

function WindowObserver(count) {
  return new Promise(resolve => {
    let windowObserver = function(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened") {
        return;
      }

      if (--count == 0) {
        Services.ww.unregisterNotification(windowObserver);
        resolve();
      }
    };
    Services.ww.registerNotification(windowObserver);
  });
}

add_task(async _ => {
  info("All opened if the pref is off");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", false],
      ["dom.disable_open_during_load", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let obs = new WindowObserver(2);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#openPopups",
    {},
    tab.linkedBrowser
  );

  await obs;
  ok(true, "We had 2 windows.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async _ => {
  info("2 window.open()s in a click event allowed because whitelisted domain.");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  const uri = Services.io.newURI(TEST_DOMAIN);
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  Services.perms.addFromPrincipal(
    principal,
    "popup",
    Services.perms.ALLOW_ACTION
  );

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let obs = new WindowObserver(2);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#openPopups",
    {},
    tab.linkedBrowser
  );

  await obs;
  ok(true, "We had 2 windows.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
});

add_task(async _ => {
  info(
    "2 window.open()s in a mouseup event allowed because whitelisted domain."
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  const uri = Services.io.newURI(TEST_DOMAIN);
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  Services.perms.addFromPrincipal(
    principal,
    "popup",
    Services.perms.ALLOW_ACTION
  );

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let obs = new WindowObserver(2);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#input",
    { type: "mouseup" },
    tab.linkedBrowser
  );

  await obs;
  ok(true, "We had 2 windows.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
});

add_task(async _ => {
  info(
    "2 window.open()s in a single click event: only the first one is allowed."
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let p = ContentTask.spawn(browser, null, () => {
    return new content.Promise(resolve => {
      content.addEventListener(
        "DOMPopupBlocked",
        () => {
          ok(true, "The popup has been blocked");
          resolve();
        },
        { once: true }
      );
    });
  });

  let obs = new WindowObserver(1);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#openPopups",
    {},
    tab.linkedBrowser
  );

  await p;
  await obs;
  ok(true, "We had only 1 window.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async _ => {
  info(
    "2 window.open()s in a single mouseup event: only the first one is allowed."
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let p = ContentTask.spawn(browser, null, () => {
    return new content.Promise(resolve => {
      content.addEventListener(
        "DOMPopupBlocked",
        () => {
          ok(true, "The popup has been blocked");
          resolve();
        },
        { once: true }
      );
    });
  });

  let obs = new WindowObserver(1);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#input",
    { type: "mouseup" },
    tab.linkedBrowser
  );

  await p;
  await obs;
  ok(true, "We had only 1 window.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async _ => {
  info("2 window.open()s by non-event code: no windows allowed.");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html?openPopups"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, null, () => {
    return new content.Promise(resolve => {
      let count = 0;
      content.addEventListener("DOMPopupBlocked", function cb() {
        if (++count == 2) {
          content.removeEventListener("DOMPopupBlocked", cb);
          ok(true, "The popup has been blocked");
          resolve();
        }
      });

      let p = content.document.createElement("p");
      p.setAttribute("id", "start");
      content.document.body.appendChild(p);
    });
  });

  ok(true, "We had 0 windows.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async _ => {
  info("2 window.open()s by non-event code allowed by permission");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  const uri = Services.io.newURI(TEST_DOMAIN);
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  Services.perms.addFromPrincipal(
    principal,
    "popup",
    Services.perms.ALLOW_ACTION
  );

  let obs = new WindowObserver(2);

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html?openPopups"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  ok(true, "We had 2 windows.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
});

add_task(async _ => {
  info(
    "1 window.open() executing another window.open(): only the first one is allowed."
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // We don't receive DOMPopupBlocked for nested windows. Let's use just the observer.
  let obs = new WindowObserver(1);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#openNestedPopups",
    {},
    tab.linkedBrowser
  );

  await obs;
  ok(true, "We had 1 window.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async _ => {
  info("window.open() and .click() on the element opening the window.");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let p = ContentTask.spawn(browser, null, () => {
    return new content.Promise(resolve => {
      content.addEventListener(
        "DOMPopupBlocked",
        () => {
          ok(true, "The popup has been blocked");
          resolve();
        },
        { once: true }
      );
    });
  });

  let obs = new WindowObserver(1);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#openPopupAndClick",
    {},
    tab.linkedBrowser
  );

  await p;
  await obs;
  ok(true, "We had only 1 window.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async _ => {
  info("All opened from chrome.");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(
    gBrowser,
    CHROME_DOMAIN + TEST_PATH + "browser_multiple_popups.html"
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let obs = new WindowObserver(2);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#openPopups",
    {},
    tab.linkedBrowser
  );

  await obs;
  ok(true, "We had 2 windows.");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#closeAllWindows",
    {},
    tab.linkedBrowser
  );

  BrowserTestUtils.removeTab(tab);
});
