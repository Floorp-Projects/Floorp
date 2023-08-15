/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

const gTests = [
  {
    name: "test contextmenu on password field in create login view",
    async setup(browser) {
      // load up the create login view
      await SpecialPowers.spawn(browser, [], async () => {
        let loginList = Cu.waiveXrays(
          content.document.querySelector("login-list")
        );
        let createButton = loginList._createLoginButton;
        createButton.click();
      });
    },
  },
];

if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
  gTests[gTests.length] = {
    name: "test contextmenu on password field in edit login view",
    async setup(browser) {
      let osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);

      // load up the edit login view
      await SpecialPowers.spawn(
        browser,
        [LoginHelper.loginToVanillaObject(TEST_LOGIN1)],
        async login => {
          let loginList = content.document.querySelector("login-list");
          let loginListItem = loginList.shadowRoot.querySelector(
            "login-list-item[data-guid]:not([hidden])"
          );
          info("Clicking on the first login");

          loginListItem.click();
          let loginItem = Cu.waiveXrays(
            content.document.querySelector("login-item")
          );
          await ContentTaskUtils.waitForCondition(() => {
            return (
              loginItem._login.guid == loginListItem.dataset.guid &&
              loginItem._login.guid == login.guid
            );
          }, "Waiting for login item to get populated");
          let editButton = loginItem.shadowRoot
            .querySelector(".edit-button")
            .shadowRoot.querySelector("button");
          editButton.click();
        }
      );
      await osAuthDialogShown;
      await SpecialPowers.spawn(browser, [], async () => {
        let loginItem = Cu.waiveXrays(
          content.document.querySelector("login-item")
        );
        await ContentTaskUtils.waitForCondition(
          () => loginItem.dataset.editing,
          "Waiting for login-item to be in editing state"
        );
      });
    },
  };
}

/**
 * Synthesize mouse clicks to open the password manager context menu popup
 * for a target input element.
 *
 */
async function openContextMenuForPasswordInput(browser) {
  const doc = browser.ownerDocument;
  const CONTEXT_MENU = doc.getElementById("contentAreaContextMenu");

  let contextMenuShownPromise = BrowserTestUtils.waitForEvent(
    CONTEXT_MENU,
    "popupshown"
  );

  let passwordInputCoords = await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));

    // The password display field is in the DOM when password input is unfocused.
    // To get the password input field, ensure it receives focus.
    let passwordInput = loginItem.shadowRoot.querySelector(
      "input[type='password']"
    );
    passwordInput.focus();
    passwordInput = loginItem.shadowRoot.querySelector(
      "input[name='password']"
    );

    passwordInput.focus();
    let passwordRect = passwordInput.getBoundingClientRect();

    // listen for the contextmenu event so we can assert on receiving it
    // and examine the target
    content.contextmenuPromise = new Promise(resolve => {
      content.document.body.addEventListener(
        "contextmenu",
        event => {
          info(
            `Received event on target: ${event.target.nodeName}, type: ${event.target.type}`
          );
          content.console.log("got contextmenu event: ", event);
          resolve(event);
        },
        { once: true }
      );
    });

    let coords = {
      x: passwordRect.x + passwordRect.width / 2,
      y: passwordRect.y + passwordRect.height / 2,
    };
    return coords;
  });

  // add the offsets of the <browser> in the chrome window
  let browserOffsets = browser.getBoundingClientRect();
  let offsetX = browserOffsets.x + passwordInputCoords.x;
  let offsetY = browserOffsets.y + passwordInputCoords.y;

  // Synthesize a right mouse click over the password input element, we have to trigger
  // both events because formfill code relies on this event happening before the contextmenu
  // (which it does for real user input) in order to not show the password autocomplete.
  let eventDetails = { type: "mousedown", button: 2 };
  await EventUtils.synthesizeMouseAtPoint(offsetX, offsetY, eventDetails);

  // Synthesize a contextmenu event to actually open the context menu.
  eventDetails = { type: "contextmenu", button: 2 };
  await EventUtils.synthesizeMouseAtPoint(offsetX, offsetY, eventDetails);

  await SpecialPowers.spawn(browser, [], async () => {
    let event = await content.contextmenuPromise;
    // XXX the event target here is the login-item element,
    //     not the input[type='password'] in its shadowRoot
    info("contextmenu event target: " + event.target.nodeName);
  });

  info("waiting for contextMenuShownPromise");
  await contextMenuShownPromise;
  return CONTEXT_MENU;
}

async function testContextMenuOnInputField(testData) {
  let browser = gBrowser.selectedBrowser;

  await SimpleTest.promiseFocus(browser.ownerGlobal);
  await testData.setup(browser);

  info("test setup completed");
  let contextMenu = await openContextMenuForPasswordInput(browser);
  let fillItem = contextMenu.querySelector("#fill-login");
  Assert.ok(fillItem, "fill menu item exists");
  Assert.ok(
    fillItem && EventUtils.isHidden(fillItem),
    "fill menu item is hidden"
  );

  let promiseHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  info("Calling hidePopup on contextMenu");
  contextMenu.hidePopup();
  info("waiting for promiseHidden");
  await promiseHidden;
}

for (let testData of gTests) {
  let tmp = {
    async [testData.name]() {
      await testContextMenuOnInputField(testData);
    },
  };
  add_task(tmp[testData.name]);
}
