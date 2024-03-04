add_task(async function () {
  var privWin = OpenBrowserWindow({ private: true });
  await new privWin.Promise(resolve => {
    privWin.addEventListener(
      "load",
      function () {
        resolve();
      },
      { once: true }
    );
  });

  var pubWin = OpenBrowserWindow({ private: false });
  await new pubWin.Promise(resolve => {
    pubWin.addEventListener(
      "load",
      function () {
        resolve();
      },
      { once: true }
    );
  });

  var URL =
    "http://mochi.test:8888/browser/dom/tests/browser/page_privatestorageevent.html";

  var privTab = BrowserTestUtils.addTab(privWin.gBrowser, URL);
  await BrowserTestUtils.browserLoaded(
    privWin.gBrowser.getBrowserForTab(privTab)
  );
  var privBrowser = gBrowser.getBrowserForTab(privTab);

  var pubTab = BrowserTestUtils.addTab(pubWin.gBrowser, URL);
  await BrowserTestUtils.browserLoaded(
    pubWin.gBrowser.getBrowserForTab(pubTab)
  );
  var pubBrowser = gBrowser.getBrowserForTab(pubTab);

  // Check if pubWin can see privWin's storage events
  await SpecialPowers.spawn(pubBrowser, [], function () {
    content.window.gotStorageEvent = false;
    content.window.addEventListener("storage", () => {
      content.window.gotStorageEvent = true;
    });
  });

  await SpecialPowers.spawn(privBrowser, [], function () {
    content.window.localStorage.key = "ablooabloo";
  });

  let pubSaw = await SpecialPowers.spawn(pubBrowser, [], function () {
    return content.window.gotStorageEvent;
  });

  ok(!pubSaw, "pubWin shouldn't be able to see privWin's storage events");

  await SpecialPowers.spawn(privBrowser, [], function () {
    content.window.gotStorageEvent = false;
    content.window.addEventListener("storage", () => {
      content.window.gotStorageEvent = true;
    });
  });

  // Check if privWin can see pubWin's storage events
  await SpecialPowers.spawn(privBrowser, [], function () {
    content.window.gotStorageEvent = false;
    content.window.addEventListener("storage", () => {
      content.window.gotStorageEvent = true;
    });
  });

  await SpecialPowers.spawn(pubBrowser, [], function () {
    content.window.localStorage.key = "ablooabloo";
  });

  let privSaw = await SpecialPowers.spawn(privBrowser, [], function () {
    return content.window.gotStorageEvent;
  });

  ok(!privSaw, "privWin shouldn't be able to see pubWin's storage events");

  BrowserTestUtils.removeTab(privTab);
  await BrowserTestUtils.closeWindow(privWin);

  BrowserTestUtils.removeTab(pubTab);
  await BrowserTestUtils.closeWindow(pubWin);
});
