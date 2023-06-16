/**
 * In this test, we check that the content can't open more than one popup at a
 * time (depending on "dom.allow_mulitple_popups" preference value).
 */

const TEST_DOMAIN = "https://example.net";
const TEST_PATH = "/browser/dom/base/test/";
const CHROME_DOMAIN = "chrome://mochitests/content";

requestLongerTimeout(2);

function promisePopups(count) {
  let windows = [];
  return new Promise(resolve => {
    if (count == 0) {
      resolve([]);
      return;
    }

    let windowObserver = function (aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened") {
        return;
      }
      windows.push(aSubject);
      if (--count == 0) {
        Services.ww.unregisterNotification(windowObserver);
        SimpleTest.executeSoon(() => resolve(windows));
      }
    };
    Services.ww.registerNotification(windowObserver);
  });
}

async function withTestPage(popupCount, optionsOrCallback, callback) {
  let options = optionsOrCallback;
  if (!callback) {
    callback = optionsOrCallback;
    options = {};
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  let domain = options.chrome ? CHROME_DOMAIN : TEST_DOMAIN;
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    domain + TEST_PATH + "browser_multiple_popups.html" + (options.query || "")
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let obs = promisePopups(popupCount);

  await callback(browser);

  let windows = await obs;
  ok(true, `We had ${popupCount} windows.`);
  for (let win of windows) {
    if (win.document.readyState !== "complete") {
      await BrowserTestUtils.waitForEvent(win, "load");
    }
    await BrowserTestUtils.closeWindow(win);
  }
  BrowserTestUtils.removeTab(tab);
}

function promisePopupsBlocked(browser, expectedCount) {
  return SpecialPowers.spawn(browser, [expectedCount], count => {
    return new content.Promise(resolve => {
      content.addEventListener("DOMPopupBlocked", function cb() {
        if (--count == 0) {
          content.removeEventListener("DOMPopupBlocked", cb);
          ok(true, "The popup has been blocked");
          resolve();
        }
      });
    });
  });
}

function startOpeningTwoPopups(browser) {
  return SpecialPowers.spawn(browser.browsingContext, [], () => {
    let p = content.document.createElement("p");
    p.setAttribute("id", "start");
    content.document.body.appendChild(p);
  });
}

add_task(async _ => {
  info("All opened if the pref is off");
  await withTestPage(2, async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.block_multiple_popups", false],
        ["dom.disable_open_during_load", true],
      ],
    });

    await BrowserTestUtils.synthesizeMouseAtCenter("#openPopups", {}, browser);
  });
});

add_task(async _ => {
  info("2 window.open()s in a click event allowed because whitelisted domain.");

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

  await withTestPage(2, async function (browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter("#openPopups", {}, browser);
  });

  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });
});

add_task(async _ => {
  info(
    "2 window.open()s in a mouseup event allowed because whitelisted domain."
  );

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

  await withTestPage(2, async function (browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter("#input", {}, browser);
  });

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

  await withTestPage(1, async function (browser) {
    let p = promisePopupsBlocked(browser, 1);
    await BrowserTestUtils.synthesizeMouseAtCenter("#openPopups", {}, browser);
    await p;
  });
});

add_task(async _ => {
  info(
    "2 window.open()s in a single mouseup event: only the first one is allowed."
  );

  await withTestPage(1, async function (browser) {
    let p = promisePopupsBlocked(browser, 1);
    await BrowserTestUtils.synthesizeMouseAtCenter("#input", {}, browser);
    await p;
  });
});

add_task(async _ => {
  info("2 window.open()s by non-event code: no windows allowed.");

  await withTestPage(0, { query: "?openPopups" }, async function (browser) {
    let p = promisePopupsBlocked(browser, 2);
    await startOpeningTwoPopups(browser);
    await p;
  });
});

add_task(async _ => {
  info("2 window.open()s by non-event code allowed by permission");
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

  await withTestPage(2, { query: "?openPopups" }, async function (browser) {
    await startOpeningTwoPopups(browser);
  });

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

  await withTestPage(1, async function (browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#openNestedPopups",
      {},
      browser
    );
  });
});

add_task(async _ => {
  info("window.open() and .click() on the element opening the window.");

  await withTestPage(1, async function (browser) {
    let p = promisePopupsBlocked(browser, 1);

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#openPopupAndClick",
      {},
      browser
    );

    await p;
  });
});

add_task(async _ => {
  info("All opened from chrome.");
  await withTestPage(2, { chrome: true }, async function (browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter("#openPopups", {}, browser);
  });
});

add_task(async function test_bug_1685056() {
  info(
    "window.open() from a blank iframe window during an event dispatched at the parent page: window should be allowed"
  );

  await withTestPage(1, async function (browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#openPopupInFrame",
      {},
      browser
    );
  });
});

add_task(async function test_bug_1689853() {
  info("window.open() from a js bookmark (LOAD_FLAGS_ALLOW_POPUPS)");
  await withTestPage(1, async function (browser) {
    const URI =
      "javascript:void(window.open('empty.html', '_blank', 'width=100,height=100'));";
    window.openTrustedLinkIn(URI, "current", {
      allowPopups: true,
      inBackground: false,
      allowInheritPrincipal: true,
    });
  });
});
