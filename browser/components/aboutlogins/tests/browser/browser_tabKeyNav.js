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

add_setup(async function() {
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
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    // list [selector, shadow root selector] for each element
    // in the order we expect them to be navigated.
    let expectedElementsInOrder = [
      ["login-filter", "input"],
      ["fxaccounts-button", "button"],
      ["menu-button", "button"],
      ["login-list", "select"],
      ["login-list", "ol"],
      ["login-list", "button"],
      ["login-item", "button.edit-button"],
      ["login-item", "button.delete-button"],
      ["login-item", "a.origin-input"],
      ["login-item", "button.copy-username-button"],
      ["login-item", "input.reveal-password-checkbox"],
      ["login-item", "button.copy-password-button"],
    ];

    let firstElement = content.document
      .querySelector(expectedElementsInOrder.at(0).at(0))
      .shadowRoot.querySelector(expectedElementsInOrder.at(0).at(1));
    let lastElement = content.document
      .querySelector(expectedElementsInOrder.at(-1).at(0))
      .shadowRoot.querySelector(expectedElementsInOrder.at(-1).at(1));

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
    // Helper function for getting the DOM element given an entry in the ordered list
    function getElementFromOrderedArray(combinedSelectors) {
      let [selector, shadowRootSelector] = combinedSelectors;
      return content.document
        .querySelector(selector)
        .shadowRoot.querySelector(shadowRootSelector);
    }
    // Ensure the test starts in a valid state
    firstElement.focus();
    // Assert that we tab navigate correctly
    for (let expectedSelector of expectedElementsInOrder) {
      let expectedElement = getElementFromOrderedArray(expectedSelector);
      // By default, MacOS will skip over certain text controls, such as links.
      if (
        content.window.navigator.platform.toLowerCase().includes("mac") &&
        expectedElement.tagName === "A"
      ) {
        continue;
      }
      let actualElem = getFocusedElement();
      is(
        actualElem,
        expectedElement,
        "Actual focused element should equal the expected focused element"
      );
      await tab();
    }

    lastElement.focus();

    // Assert that we shift + tab navigate correctly starting from the last ordered element
    for (let expectedSelector of expectedElementsInOrder.reverse()) {
      let expectedElement = getElementFromOrderedArray(expectedSelector);
      // By default, MacOS will skip over certain text controls, such as links.
      if (
        content.window.navigator.platform.toLowerCase().includes("mac") &&
        expectedElement.tagName === "A"
      ) {
        continue;
      }
      let actualElement = getFocusedElement();
      is(
        actualElement,
        expectedElement,
        "Actual focused element should equal the expected focused element"
      );
      await shiftTab();
    }
  });
});

add_task(async function testTabToCreateButton() {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);

    function waitForAnimationFrame() {
      return new Promise(resolve => content.requestAnimationFrame(resolve));
    }

    async function tab() {
      EventUtils.synthesizeKey("KEY_Tab", {}, content);
      await waitForAnimationFrame();
    }

    let loginList = content.document.querySelector("login-list");
    let loginSort = loginList.shadowRoot.getElementById("login-sort");
    let loginListbox = loginList.shadowRoot.querySelector("ol");
    let createButton = loginList.shadowRoot.querySelector(
      ".create-login-button"
    );
    let getFocusedElement = () => loginList.shadowRoot.activeElement;

    is(getFocusedElement(), null, "login-list isn't focused");

    loginSort.focus();
    await waitForAnimationFrame();
    is(getFocusedElement(), loginSort, "login sort is focused");

    await tab();
    is(getFocusedElement(), loginListbox, "listbox is focused next");

    await tab();
    is(getFocusedElement(), createButton, "create button is after");

    await tab();
    is(getFocusedElement(), null, "login-list isn't focused again");
  });
});

add_task(async function testTabToEditButton() {
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

      let loginList = content.document.querySelector("login-list");
      let loginItem = content.document.querySelector("login-item");
      let loginFilter = content.document.querySelector("login-filter");
      let createButton = loginList.shadowRoot.querySelector(
        ".create-login-button"
      );
      let editButton = loginItem.shadowRoot.querySelector(".edit-button");
      let breachAlert = loginItem.shadowRoot.querySelector(".breach-alert");
      let getFocusedElement = () => {
        if (content.document.activeElement == loginList) {
          return loginList.shadowRoot.activeElement;
        }
        if (content.document.activeElement == loginItem) {
          return loginItem.shadowRoot.activeElement;
        }
        if (content.document.activeElement == loginFilter) {
          return loginFilter.shadowRoot.activeElement;
        }
        ok(
          false,
          "not expecting a different element to get focused in this test: " +
            content.document.activeElement.outerHTML
        );
        return undefined;
      };

      for (let guidToSelect of [testLoginNormalGuid, testLoginBreachedGuid]) {
        let loginListItem = loginList.shadowRoot.querySelector(
          `.login-list-item[data-guid="${guidToSelect}"]`
        );
        loginListItem.click();
        await ContentTaskUtils.waitForCondition(() => {
          let waivedLoginItem = Cu.waiveXrays(loginItem);
          return (
            waivedLoginItem._login &&
            waivedLoginItem._login.guid == guidToSelect
          );
        }, "waiting for login-item to show the selected login");

        is(
          breachAlert.hidden,
          guidToSelect == testLoginNormalGuid,
          ".breach-alert should be hidden if the login is not breached. current login breached? " +
            (guidToSelect == testLoginBreachedGuid)
        );

        createButton.focus();
        await waitForAnimationFrame();
        is(getFocusedElement(), createButton, "create button is focused");

        await tab();
        await waitForAnimationFrame();
        is(getFocusedElement(), editButton, "edit button is focused");
      }
    }
  );
});
