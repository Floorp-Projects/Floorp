/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

EXPECTED_BREACH = {
  AddedDate: "2018-12-20T23:56:26Z",
  BreachDate: "2018-12-16",
  Domain: "breached.example.com",
  Name: "Breached",
  PwnCount: 1643100,
  DataClasses: ["Email addresses", "Usernames", "Passwords", "IP addresses"],
  _status: "synced",
  id: "047940fe-d2fd-4314-b636-b4a952ee0043",
  last_modified: "1541615610052",
  schema: "1541615609018",
};

add_setup(async function () {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_tab_key_nav() {
  const browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    // Helper function for getting the resulting DOM element given a list of selectors possibly inside shadow DOM
    const selectWithShadowRootIfNeeded = (document, selectorsArray) =>
      selectorsArray.reduce(
        (selectionSoFar, currentSelector) =>
          selectionSoFar.shadowRoot
            ? selectionSoFar.shadowRoot.querySelector(currentSelector)
            : selectionSoFar.querySelector(currentSelector),
        document
      );

    const EventUtils = ContentTaskUtils.getEventUtils(content);
    // list [selector, shadow root selector] for each element
    // in the order we expect them to be navigated.
    const expectedElementsInOrder = [
      ["login-list", "login-filter", "input"],
      ["login-list", "create-login-button", "button"],
      ["login-list", "select#login-sort"],
      ["login-list", "ol"],
      ["login-item", "edit-button", "button"],
      ["login-item", "delete-button", "button"],
      ["login-item", "a.origin-input"],
      ["login-item", "copy-username-button", "button"],
      ["login-item", "input.reveal-password-checkbox"],
      ["login-item", "copy-password-button", "button"],
    ];

    const firstElement = selectWithShadowRootIfNeeded(
      content.document,
      expectedElementsInOrder.at(0)
    );

    const lastElement = selectWithShadowRootIfNeeded(
      content.document,
      expectedElementsInOrder.at(-1)
    );

    async function tab() {
      EventUtils.synthesizeKey("KEY_Tab", {}, content);
      await new Promise(resolve => content.requestAnimationFrame(resolve));
      // The following line can help with focus trap debugging:
      // await new Promise(resolve => content.window.setTimeout(resolve, 500));
    }
    async function shiftTab() {
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true }, content);
      await new Promise(resolve => content.requestAnimationFrame(resolve));
      // await new Promise(resolve => content.window.setTimeout(resolve, 500));
    }

    // Getting focused shadow DOM element itself instead of shadowRoot,
    // using recursion for any component-nesting level, as in:
    // document.activeElement.shadowRoot.activeElement.shadowRoot.activeElement
    function getFocusedElement() {
      let element = content.document.activeElement;
      const getShadowRootFocus = e => {
        if (e.shadowRoot) {
          return getShadowRootFocus(e.shadowRoot.activeElement);
        }
        return e;
      };
      return getShadowRootFocus(element);
    }

    // Ensure the test starts in a valid state
    firstElement.focus();
    // Assert that we tab navigate correctly
    for (let expectedSelector of expectedElementsInOrder) {
      const expectedElement = selectWithShadowRootIfNeeded(
        content.document,
        expectedSelector
      );

      // By default, MacOS will skip over certain text controls, such as links.
      if (
        content.window.navigator.platform.toLowerCase().includes("mac") &&
        expectedElement.tagName === "A"
      ) {
        continue;
      }

      const actualElement = getFocusedElement();

      Assert.equal(
        actualElement,
        expectedElement,
        "Actual focused element should equal the expected focused element"
      );
      await tab();
    }

    lastElement.focus();

    // Assert that we shift + tab navigate correctly starting from the last ordered element
    for (let expectedSelector of expectedElementsInOrder.reverse()) {
      const expectedElement = selectWithShadowRootIfNeeded(
        content.document,
        expectedSelector
      );
      // By default, MacOS will skip over certain text controls, such as links.
      if (
        content.window.navigator.platform.toLowerCase().includes("mac") &&
        expectedElement.tagName === "A"
      ) {
        continue;
      }

      const actualElement = getFocusedElement();
      Assert.equal(
        actualElement,
        expectedElement,
        "Actual focused element should equal the expected focused element"
      );
      await shiftTab();
    }
    await tab(); // tab back to the first element
  });
});

add_task(async function test_tab_to_create_button() {
  const browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);

    function waitForAnimationFrame() {
      return new Promise(resolve => content.requestAnimationFrame(resolve));
    }

    async function tab() {
      EventUtils.synthesizeKey("KEY_Tab", {}, content);
      await waitForAnimationFrame();
    }

    const loginList = content.document.querySelector("login-list");
    const loginFilter = loginList.shadowRoot.querySelector("login-filter");
    const loginSort = loginList.shadowRoot.getElementById("login-sort");
    const loginListbox = loginList.shadowRoot.querySelector("ol");
    const createButton = loginList.shadowRoot.querySelector(
      "create-login-button"
    );

    const getFocusedElement = () => loginList.shadowRoot.activeElement;
    Assert.equal(getFocusedElement(), loginFilter, "login-filter is focused");

    await tab();
    Assert.equal(getFocusedElement(), createButton, "create button is focused");

    await tab();
    Assert.equal(getFocusedElement(), loginSort, "login sort is focused");

    await tab();
    Assert.equal(getFocusedElement(), loginListbox, "listbox is focused next");

    await tab();
    Assert.equal(getFocusedElement(), null, "login-list isn't focused again");
  });
});

add_task(async function test_tab_to_edit_button() {
  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(
    browser,
    [[TEST_LOGIN1.guid, TEST_LOGIN3.guid]],
    async ([testLoginNormalGuid, testLoginBreachedGuid]) => {
      const EventUtils = ContentTaskUtils.getEventUtils(content);

      function waitForAnimationFrame() {
        return new Promise(resolve => content.requestAnimationFrame(resolve));
      }

      async function tab() {
        EventUtils.synthesizeKey("KEY_Tab", {}, content);
        await waitForAnimationFrame();
      }

      const loginList = content.document.querySelector("login-list");
      const loginItem = content.document.querySelector("login-item");
      const loginFilter = loginList.shadowRoot.querySelector("login-filter");
      const createButton = loginList.shadowRoot.querySelector(
        "create-login-button"
      );
      const loginSort = loginList.shadowRoot.getElementById("login-sort");
      const loginListbox = loginList.shadowRoot.querySelector("ol");
      const editButton = loginItem.shadowRoot.querySelector("edit-button");
      const breachAlert =
        loginItem.shadowRoot.querySelector("login-breach-alert");
      const getFocusedElement = () => {
        if (content.document.activeElement == loginList) {
          return loginList.shadowRoot.activeElement;
        }
        if (content.document.activeElement == loginItem) {
          return loginItem.shadowRoot.activeElement;
        }
        if (content.document.activeElement == loginFilter) {
          return loginFilter.shadowRoot.activeElement;
        }
        Assert.ok(
          false,
          "not expecting a different element to get focused in this test: " +
            content.document.activeElement.outerHTML
        );
        return undefined;
      };

      for (let guidToSelect of [testLoginNormalGuid, testLoginBreachedGuid]) {
        let loginListItem = loginList.shadowRoot.querySelector(
          `login-list-item[data-guid="${guidToSelect}"]`
        );
        loginListItem.click();
        await ContentTaskUtils.waitForCondition(() => {
          let waivedLoginItem = Cu.waiveXrays(loginItem);
          return (
            waivedLoginItem._login &&
            waivedLoginItem._login.guid == guidToSelect
          );
        }, "waiting for login-item to show the selected login");

        Assert.equal(
          breachAlert.hidden,
          guidToSelect == testLoginNormalGuid,
          ".breach-alert should be hidden if the login is not breached. current login breached? " +
            (guidToSelect == testLoginBreachedGuid)
        );

        createButton.shadowRoot.querySelector("button").focus();
        Assert.equal(
          getFocusedElement(),
          createButton,
          "create button is focused"
        );

        await tab();
        Assert.equal(getFocusedElement(), loginSort, "login sort is focused");

        await tab();
        Assert.equal(
          getFocusedElement(),
          loginListbox,
          "listbox is focused next"
        );

        await tab();
        Assert.equal(getFocusedElement(), editButton, "edit button is focused");
      }
    }
  );
});
