/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

const SIDEBAR_CLOSED_COUNT_PREF =
  "browser.shopping.experience2023.sidebarClosedCount";
const SHOW_KEEP_SIDEBAR_CLOSED_MESSAGE_PREF =
  "browser.shopping.experience2023.showKeepSidebarClosedMessage";
const SHOPPING_SIDEBAR_ACTIVE_PREF = "browser.shopping.experience2023.active";
const SIDEBAR_AUTO_OPEN_ENABLED_PREF =
  "browser.shopping.experience2023.autoOpen.enabled";
const SIDEBAR_AUTO_OPEN_USER_ENABLED_PREF =
  "browser.shopping.experience2023.autoOpen.userEnabled";
const SHOPPING_OPTED_IN_PREF = "browser.shopping.experience2023.optedIn";

add_task(
  async function test_keep_close_message_bar_no_longer_shows_after_3_appearences() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [SHOPPING_SIDEBAR_ACTIVE_PREF, true],
        [SHOW_KEEP_SIDEBAR_CLOSED_MESSAGE_PREF, true],
        [SHOPPING_OPTED_IN_PREF, 1],
        [SIDEBAR_CLOSED_COUNT_PREF, 3],
        [SIDEBAR_AUTO_OPEN_ENABLED_PREF, true],
        [SIDEBAR_AUTO_OPEN_USER_ENABLED_PREF, true],
      ],
    });

    await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async browser => {
      let shoppingButton = document.getElementById("shopping-sidebar-button");
      let browserPanel = gBrowser.getPanel(browser);

      let sidebar = browserPanel.querySelector("shopping-sidebar");

      function waitForSidebarOpen() {
        return BrowserTestUtils.waitForMutationCondition(
          shoppingButton,
          {
            attributeFilter: ["shoppingsidebaropen"],
          },
          () => shoppingButton.getAttribute("shoppingsidebaropen") === "true"
        );
      }

      function waitForSidebarClosed() {
        return BrowserTestUtils.waitForMutationCondition(
          shoppingButton,
          {
            attributeFilter: ["shoppingsidebaropen"],
          },
          () => shoppingButton.getAttribute("shoppingsidebaropen") === "false"
        );
      }

      function assertKeepClosedMessageBarVisible() {
        return SpecialPowers.spawn(
          sidebar.querySelector("browser"),
          [],
          async () => {
            let shoppingContainer =
              content.document.querySelector(
                "shopping-container"
              ).wrappedJSObject;
            await shoppingContainer.updateComplete;

            await ContentTaskUtils.waitForCondition(() => {
              return (
                !!shoppingContainer.keepClosedMessageBarEl &&
                ContentTaskUtils.isVisible(
                  shoppingContainer.keepClosedMessageBarEl
                )
              );
            }, "Waiting for keep message bar to be visible");

            await shoppingContainer.keepClosedMessageBarEl.updateComplete;

            Assert.ok(
              shoppingContainer.showingKeepClosedMessage,
              "We are showing the keep closed message bar"
            );
          }
        );
      }

      function assertKeepClosedMessageBarNotShowing() {
        return SpecialPowers.spawn(
          sidebar.querySelector("browser"),
          [],
          async () => {
            let shoppingContainer =
              content.document.querySelector(
                "shopping-container"
              ).wrappedJSObject;
            await shoppingContainer.updateComplete;

            await ContentTaskUtils.waitForCondition(() => {
              return (
                !shoppingContainer.keepClosedMessageBarEl ||
                ContentTaskUtils.isHidden(
                  shoppingContainer.keepClosedMessageBarEl
                )
              );
            }, "Waiting for keep message bar to be visible");

            Assert.ok(
              !shoppingContainer.showingKeepClosedMessage,
              "We are not showing the keep closed message bar"
            );
          }
        );
      }

      function clickSidebarCloseButton() {
        return SpecialPowers.spawn(
          sidebar.querySelector("browser"),
          [],
          async () => {
            let shoppingContainer =
              content.document.querySelector(
                "shopping-container"
              ).wrappedJSObject;
            await shoppingContainer.updateComplete;

            shoppingContainer.closeButtonEl.click();
          }
        );
      }

      await promiseSidebarUpdated(sidebar, PRODUCT_PAGE);

      await waitForSidebarOpen();
      ok(
        BrowserTestUtils.isVisible(sidebar),
        "Shopping sidebar should be open"
      );

      // Close sidebar
      shoppingButton.click();

      await waitForSidebarClosed();
      ok(
        BrowserTestUtils.isHidden(sidebar),
        "Shopping sidebar should be closed"
      );

      // Open sidebar
      shoppingButton.click();

      await waitForSidebarOpen();
      ok(
        BrowserTestUtils.isVisible(sidebar),
        "Shopping sidebar should be open"
      );

      // Try closing sidebar. Keep closed message bar will show
      await clickSidebarCloseButton();

      await TestUtils.waitForTick();
      await assertKeepClosedMessageBarVisible();

      await clickSidebarCloseButton();

      await waitForSidebarClosed();
      ok(
        BrowserTestUtils.isHidden(sidebar),
        "Shopping sidebar should be closed"
      );

      // Open sidebar
      shoppingButton.click();

      await waitForSidebarOpen();
      ok(
        BrowserTestUtils.isVisible(sidebar),
        "Shopping sidebar should be open"
      );

      // Try closing sidebar. Keep closed message bar will show
      shoppingButton.click();

      await TestUtils.waitForTick();
      await assertKeepClosedMessageBarVisible();

      shoppingButton.click();

      await waitForSidebarClosed();
      ok(
        BrowserTestUtils.isHidden(sidebar),
        "Shopping sidebar should be closed"
      );

      // Open sidebar
      shoppingButton.click();

      await waitForSidebarOpen();
      ok(
        BrowserTestUtils.isVisible(sidebar),
        "Shopping sidebar should be open"
      );

      // Try closing sidebar. Keep closed message bar will show
      shoppingButton.click();

      await TestUtils.waitForTick();
      await assertKeepClosedMessageBarVisible();

      shoppingButton.click();

      await waitForSidebarClosed();
      ok(
        BrowserTestUtils.isHidden(sidebar),
        "Shopping sidebar should be closed"
      );

      // Open sidebar
      shoppingButton.click();

      await waitForSidebarOpen();
      ok(
        BrowserTestUtils.isVisible(sidebar),
        "Shopping sidebar should be open"
      );
      await assertKeepClosedMessageBarNotShowing();

      // Close sidebar. Keep closed message bar no longer shows
      await clickSidebarCloseButton();

      await waitForSidebarClosed();
      ok(
        BrowserTestUtils.isHidden(sidebar),
        "Shopping sidebar should be closed"
      );
    });
  }
);

add_task(async function test_keep_close_message_bar_no_thanks() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SHOPPING_SIDEBAR_ACTIVE_PREF, true],
      [SHOW_KEEP_SIDEBAR_CLOSED_MESSAGE_PREF, true],
      [SHOPPING_OPTED_IN_PREF, 1],
      [SIDEBAR_CLOSED_COUNT_PREF, 5],
      [SIDEBAR_AUTO_OPEN_ENABLED_PREF, true],
      [SIDEBAR_AUTO_OPEN_USER_ENABLED_PREF, true],
    ],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async browser => {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    let browserPanel = gBrowser.getPanel(browser);

    let sidebar = browserPanel.querySelector("shopping-sidebar");

    function waitForSidebarOpen() {
      return BrowserTestUtils.waitForMutationCondition(
        shoppingButton,
        {
          attributeFilter: ["shoppingsidebaropen"],
        },
        () => shoppingButton.getAttribute("shoppingsidebaropen") === "true"
      );
    }

    function waitForSidebarClosed() {
      return BrowserTestUtils.waitForMutationCondition(
        shoppingButton,
        {
          attributeFilter: ["shoppingsidebaropen"],
        },
        () => shoppingButton.getAttribute("shoppingsidebaropen") === "false"
      );
    }

    function assertKeepClosedMessageBarVisible() {
      return SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          await shoppingContainer.updateComplete;

          await ContentTaskUtils.waitForCondition(() => {
            return (
              !!shoppingContainer.keepClosedMessageBarEl &&
              ContentTaskUtils.isVisible(
                shoppingContainer.keepClosedMessageBarEl
              )
            );
          }, "Waiting for keep message bar to be visible");

          await shoppingContainer.keepClosedMessageBarEl.updateComplete;

          Assert.ok(
            shoppingContainer.showingKeepClosedMessage,
            "We are showing the keep closed message bar"
          );
        }
      );
    }

    function assertKeepClosedMessageBarNotShowing() {
      return SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          await shoppingContainer.updateComplete;

          await ContentTaskUtils.waitForCondition(() => {
            return (
              !shoppingContainer.keepClosedMessageBarEl ||
              ContentTaskUtils.isHidden(
                shoppingContainer.keepClosedMessageBarEl
              )
            );
          }, "Waiting for keep message bar to be visible");

          Assert.ok(
            !shoppingContainer.showingKeepClosedMessage,
            "We are not showing the keep closed message bar"
          );
        }
      );
    }

    function clickNoThanksButton() {
      return SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          await shoppingContainer.updateComplete;

          let keepClosedMessageBar = shoppingContainer.keepClosedMessageBarEl;
          await keepClosedMessageBar.updateComplete;

          keepClosedMessageBar.noThanksButtonEl.click();
        }
      );
    }

    await promiseSidebarUpdated(sidebar, PRODUCT_PAGE);

    await waitForSidebarOpen();
    ok(BrowserTestUtils.isVisible(sidebar), "Shopping sidebar should be open");

    // Try closing sidebar. Keep closed message bar will show
    shoppingButton.click();

    await TestUtils.waitForTick();
    await assertKeepClosedMessageBarVisible();

    await clickNoThanksButton();

    await waitForSidebarClosed();
    ok(BrowserTestUtils.isHidden(sidebar), "Shopping sidebar should be closed");

    // Open sidebar
    shoppingButton.click();

    await waitForSidebarOpen();
    ok(BrowserTestUtils.isVisible(sidebar), "Shopping sidebar should be open");
    await assertKeepClosedMessageBarNotShowing();

    // Close sidebar. Keep closed message no longer shows
    shoppingButton.click();

    await waitForSidebarClosed();
    ok(BrowserTestUtils.isHidden(sidebar), "Shopping sidebar should be closed");
  });
});

add_task(async function test_keep_close_message_bar_yes_keep_closed() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SHOPPING_SIDEBAR_ACTIVE_PREF, true],
      [SHOW_KEEP_SIDEBAR_CLOSED_MESSAGE_PREF, true],
      [SHOPPING_OPTED_IN_PREF, 1],
      [SIDEBAR_CLOSED_COUNT_PREF, 5],
      [SIDEBAR_AUTO_OPEN_ENABLED_PREF, true],
      [SIDEBAR_AUTO_OPEN_USER_ENABLED_PREF, true],
    ],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async browser => {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    let browserPanel = gBrowser.getPanel(browser);

    let sidebar = browserPanel.querySelector("shopping-sidebar");

    function waitForSidebarOpen() {
      return BrowserTestUtils.waitForMutationCondition(
        shoppingButton,
        {
          attributeFilter: ["shoppingsidebaropen"],
        },
        () => shoppingButton.getAttribute("shoppingsidebaropen") === "true"
      );
    }

    function waitForSidebarClosed() {
      return BrowserTestUtils.waitForMutationCondition(
        shoppingButton,
        {
          attributeFilter: ["shoppingsidebaropen"],
        },
        () => shoppingButton.getAttribute("shoppingsidebaropen") === "false"
      );
    }

    function assertKeepClosedMessageBarVisible() {
      return SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          await shoppingContainer.updateComplete;

          await ContentTaskUtils.waitForCondition(() => {
            return (
              !!shoppingContainer.keepClosedMessageBarEl &&
              ContentTaskUtils.isVisible(
                shoppingContainer.keepClosedMessageBarEl
              )
            );
          }, "Waiting for keep message bar to be visible");

          await shoppingContainer.keepClosedMessageBarEl.updateComplete;

          Assert.ok(
            shoppingContainer.showingKeepClosedMessage,
            "We are showing the keep closed message bar"
          );
        }
      );
    }

    function assertKeepClosedMessageBarNotShowing() {
      return SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          await shoppingContainer.updateComplete;

          await ContentTaskUtils.waitForCondition(() => {
            return (
              !shoppingContainer.keepClosedMessageBarEl ||
              ContentTaskUtils.isHidden(
                shoppingContainer.keepClosedMessageBarEl
              )
            );
          }, "Waiting for keep message bar to be visible");

          Assert.ok(
            !shoppingContainer.showingKeepClosedMessage,
            "We are not showing the keep closed message bar"
          );
        }
      );
    }

    function clickYesKeepClosedButton() {
      return SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async () => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          await shoppingContainer.updateComplete;

          let keepClosedMessageBar = shoppingContainer.keepClosedMessageBarEl;
          await keepClosedMessageBar.updateComplete;

          keepClosedMessageBar.yesKeepClosedButtonEl.click();
        }
      );
    }

    await promiseSidebarUpdated(sidebar, PRODUCT_PAGE);

    await waitForSidebarOpen();
    ok(BrowserTestUtils.isVisible(sidebar), "Shopping sidebar should be open");

    // Try closing sidebar. Keep closed message bar will show
    shoppingButton.click();

    await TestUtils.waitForTick();
    await assertKeepClosedMessageBarVisible();

    await clickYesKeepClosedButton();

    await waitForSidebarClosed();
    ok(BrowserTestUtils.isHidden(sidebar), "Shopping sidebar should be closed");

    // Open sidebar
    shoppingButton.click();

    await waitForSidebarOpen();
    ok(BrowserTestUtils.isVisible(sidebar), "Shopping sidebar should be open");
    await assertKeepClosedMessageBarNotShowing();

    // Close sidebar. Keep closed message no longer shows
    shoppingButton.click();

    await waitForSidebarClosed();
    ok(BrowserTestUtils.isHidden(sidebar), "Shopping sidebar should be closed");
  });
});
