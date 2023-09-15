const TEST_PATH =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/browser/base/content/test/favicons/";

function waitForAttributeChange(tab, attr) {
  info(`Waiting for attribute ${attr}`);
  return new Promise(resolve => {
    let listener = event => {
      if (event.detail.changed.includes(attr)) {
        tab.removeEventListener("TabAttrModified", listener);
        resolve();
      }
    };

    tab.addEventListener("TabAttrModified", listener);
  });
}

function waitForPendingIcon() {
  return new Promise(resolve => {
    let listener = () => {
      LinkHandlerParent.removeListenerForTests(listener);
      resolve();
    };

    LinkHandlerParent.addListenerForTests(listener);
  });
}

// Verify that the title doesn't flicker if the icon takes too long to load.
// We expect to see events in the following order:
// "label" added to tab
// "busy" removed from tab
// icon available
// In all those cases the title should be in the same position.
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      let tab = gBrowser.getTabForBrowser(browser);
      BrowserTestUtils.startLoadingURIString(
        browser,
        TEST_PATH + "file_with_slow_favicon.html"
      );

      await waitForAttributeChange(tab, "label");
      ok(tab.hasAttribute("busy"), "Should have seen the busy attribute");
      let label = tab.textLabel;
      let bounds = label.getBoundingClientRect();

      await waitForAttributeChange(tab, "busy");
      ok(
        !tab.hasAttribute("busy"),
        "Should have seen the busy attribute removed"
      );
      let newBounds = label.getBoundingClientRect();
      is(
        bounds.x,
        newBounds.left,
        "Should have seen the title in the same place."
      );

      await waitForFaviconMessage(true);
      newBounds = label.getBoundingClientRect();
      is(
        bounds.x,
        newBounds.left,
        "Should have seen the title in the same place."
      );
    }
  );
});

// Verify that the title doesn't flicker if a new icon is detected after load.
add_task(async () => {
  let iconAvailable = waitForFaviconMessage(true);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_PATH + "blank.html" },
    async browser => {
      let icon = await iconAvailable;
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      is(icon.iconURL, "http://example.com/favicon.ico");

      let tab = gBrowser.getTabForBrowser(browser);
      let label = tab.textLabel;
      let bounds = label.getBoundingClientRect();

      await SpecialPowers.spawn(browser, [], () => {
        let link = content.document.createElement("link");
        link.setAttribute("href", "file_favicon.png");
        link.setAttribute("rel", "icon");
        link.setAttribute("type", "image/png");
        content.document.head.appendChild(link);
      });

      ok(
        !tab.hasAttribute("pendingicon"),
        "Should not have marked a pending icon"
      );
      let newBounds = label.getBoundingClientRect();
      is(
        bounds.x,
        newBounds.left,
        "Should have seen the title in the same place."
      );

      await waitForPendingIcon();

      ok(
        !tab.hasAttribute("pendingicon"),
        "Should not have marked a pending icon"
      );
      newBounds = label.getBoundingClientRect();
      is(
        bounds.x,
        newBounds.left,
        "Should have seen the title in the same place."
      );

      icon = await waitForFaviconMessage(true);
      is(
        icon.iconURL,
        TEST_PATH + "file_favicon.png",
        "Should have loaded the new icon."
      );

      ok(
        !tab.hasAttribute("pendingicon"),
        "Should not have marked a pending icon"
      );
      newBounds = label.getBoundingClientRect();
      is(
        bounds.x,
        newBounds.left,
        "Should have seen the title in the same place."
      );
    }
  );
});

// Verify that pinned tabs don't change size when an icon is pending.
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      let tab = gBrowser.getTabForBrowser(browser);
      gBrowser.pinTab(tab);

      let bounds = tab.getBoundingClientRect();
      BrowserTestUtils.startLoadingURIString(
        browser,
        TEST_PATH + "file_with_slow_favicon.html"
      );

      await waitForAttributeChange(tab, "label");
      ok(tab.hasAttribute("busy"), "Should have seen the busy attribute");
      let newBounds = tab.getBoundingClientRect();
      is(
        bounds.width,
        newBounds.width,
        "Should have seen tab remain the same size."
      );

      await waitForAttributeChange(tab, "busy");
      ok(
        !tab.hasAttribute("busy"),
        "Should have seen the busy attribute removed"
      );
      newBounds = tab.getBoundingClientRect();
      is(
        bounds.width,
        newBounds.width,
        "Should have seen tab remain the same size."
      );

      await waitForFaviconMessage(true);
      newBounds = tab.getBoundingClientRect();
      is(
        bounds.width,
        newBounds.width,
        "Should have seen tab remain the same size."
      );
    }
  );
});
