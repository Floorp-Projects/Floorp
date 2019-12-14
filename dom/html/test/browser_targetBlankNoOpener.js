const TEST_URL = "http://mochi.test:8888/browser/dom/html/test/empty.html";

async function checkOpener(browser, elm, name, rel) {
  let p = BrowserTestUtils.waitForNewTab(gBrowser, null, true, true);

  await SpecialPowers.spawn(
    browser,
    [{ url: TEST_URL, name, rel, elm }],
    async obj => {
      let element;

      if (obj.elm == "anchor") {
        element = content.document.createElement("a");
        content.document.body.appendChild(element);
        element.appendChild(content.document.createTextNode(obj.name));
      } else {
        let img = content.document.createElement("img");
        img.src = "image_yellow.png";
        content.document.body.appendChild(img);

        element = content.document.createElement("area");
        img.appendChild(element);

        element.setAttribute("shape", "rect");
        element.setAttribute("coords", "0,0,100,100");
      }

      element.setAttribute("target", "_blank");
      element.setAttribute("href", obj.url);

      if (obj.rel) {
        element.setAttribute("rel", obj.rel);
      }

      element.click();
    }
  );

  let newTab = await p;
  let newBrowser = gBrowser.getBrowserForTab(newTab);

  let hasOpener = await SpecialPowers.spawn(
    newTab.linkedBrowser,
    [],
    _ => !!content.window.opener
  );

  BrowserTestUtils.removeTab(newTab);
  return hasOpener;
}

async function runTests(browser, elm) {
  info("Creating an " + elm + " with target=_blank rel=opener");
  ok(
    !!(await checkOpener(browser, elm, "rel=opener", "opener")),
    "We want the opener with rel=opener"
  );

  info("Creating an " + elm + " with target=_blank rel=noopener");
  ok(
    !(await checkOpener(browser, elm, "rel=noopener", "noopener")),
    "We don't want the opener with rel=noopener"
  );

  info("Creating an " + elm + " with target=_blank");
  ok(
    !(await checkOpener(browser, elm, "no rel", null)),
    "We don't want the opener with no rel is passed"
  );

  info("Creating an " + elm + " with target=_blank rel='noopener opener'");
  ok(
    !(await checkOpener(
      browser,
      elm,
      "rel=noopener+opener",
      "noopener opener"
    )),
    "noopener wins with rel=noopener+opener"
  );

  info("Creating an " + elm + " with target=_blank rel='noreferrer opener'");
  ok(
    !(await checkOpener(browser, elm, "noreferrer wins", "noreferrer opener")),
    "We don't want the opener with rel=noreferrer+opener"
  );

  info("Creating an " + elm + " with target=_blank rel='opener noreferrer'");
  ok(
    !(await checkOpener(
      browser,
      elm,
      "noreferrer wins again",
      "noreferrer opener"
    )),
    "We don't want the opener with rel=opener+noreferrer"
  );
}

add_task(async _ => {
  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", false],
      ["dom.disable_open_during_load", true],
      ["dom.targetBlankNoOpener.enabled", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await runTests(browser, "anchor");
  await runTests(browser, "area");

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});
