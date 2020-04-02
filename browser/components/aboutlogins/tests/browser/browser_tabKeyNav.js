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

add_task(async function setup() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_tab_key_nav() {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);

    async function tab() {
      EventUtils.synthesizeKey("KEY_Tab", {}, content);
      await new Promise(resolve => content.requestAnimationFrame(resolve));
      // The following line can help with focus trap debugging:
      // await new Promise(resolve => content.window.setTimeout(resolve, 100));
    }

    // Getting focused shadow DOM element itself instead of shadowRoot,
    // using recursion for any component-nesting level, as in:
    // document.activeElement.shadowRoot.activeElement.shadowRoot.activeElement
    function getFocusedEl() {
      let el = content.document.activeElement;
      const getShadowRootFocus = e => {
        if (e.shadowRoot) {
          return getShadowRootFocus(e.shadowRoot.activeElement);
        }
        return e;
      };
      return getShadowRootFocus(el);
    }

    // We’re making two passes with focused and focusedAgain sets of DOM elements,
    // rather than just checking we get back to the first focused element,
    // so we can cover more edge-cases involving issues with focus tree.
    const focused = new Set();
    const focusedAgain = new Set();

    // Initializing: some element is supposed to be automatically focused
    const firstEl = getFocusedEl();
    focused.add(firstEl);

    // Setting a maximum number of keypresses to escape the loop,
    // should we fall into a focus trap.
    // Fixed amount of keypresses also helps assess consistency with keyboard navigation.
    const maxKeypresses = content.document.getElementsByTagName("*").length * 2;
    let keypresses = 1;

    while (focusedAgain.size < focused.size && keypresses <= maxKeypresses) {
      await tab();
      keypresses++;
      let el = getFocusedEl();

      // The following block is needed to get back to document
      // after tabbing to browser chrome.
      // This focus trap in browser chrome is a testing artifact we’re fixing below.
      if (el.tagName === "BODY" && el !== firstEl) {
        firstEl.focus();
        await new Promise(resolve => content.requestAnimationFrame(resolve));
        el = getFocusedEl();
      }

      if (focused.has(el)) {
        focusedAgain.add(el);
      } else {
        focused.add(el);
      }
    }

    is(
      focusedAgain.size,
      focused.size,
      "All focusable elements should be kept accessible with TAB key (no focus trap)."
    );
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
    let getFocusedEl = () => loginList.shadowRoot.activeElement;

    is(getFocusedEl(), null, "login-list isn't focused");

    loginSort.focus();
    await waitForAnimationFrame();
    is(getFocusedEl(), loginSort, "login sort is focused");

    await tab();
    is(getFocusedEl(), loginListbox, "listbox is focused next");

    await tab();
    is(getFocusedEl(), createButton, "create button is after");

    await tab();
    is(getFocusedEl(), null, "login-list isn't focused again");
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
      let createButton = loginList.shadowRoot.querySelector(
        ".create-login-button"
      );
      let editButton = loginItem.shadowRoot.querySelector(".edit-button");
      let breachAlert = loginItem.shadowRoot.querySelector(".breach-alert");
      let getFocusedEl = () => {
        if (content.document.activeElement == loginList) {
          return loginList.shadowRoot.activeElement;
        }
        if (content.document.activeElement == loginItem) {
          return loginItem.shadowRoot.activeElement;
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
        is(getFocusedEl(), createButton, "create button is focused");

        await tab();
        await waitForAnimationFrame();
        is(getFocusedEl(), editButton, "edit button is focused");
      }
    }
  );
});
