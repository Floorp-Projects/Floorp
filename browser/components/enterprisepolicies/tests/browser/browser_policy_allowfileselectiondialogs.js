/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function setup() {
  await setupPolicyEngineWithJson({
    policies: {
      AllowFileSelectionDialogs: false,
    },
  });
});

add_task(async function test_file_commands_disabled() {
  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see changes from the policy
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  let savePageCommand = newWin.document.getElementById("Browser:SavePage");
  let openFileCommand = newWin.document.getElementById("Browser:OpenFile");

  Assert.equal(
    savePageCommand.getAttribute("disabled"),
    "true",
    "Browser:SavePage command is disabled"
  );
  Assert.equal(
    openFileCommand.getAttribute("disabled"),
    "true",
    "Browser:OpenFile command is disabled"
  );
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_file_buttons_disabled() {
  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see changes from the policy
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  newWin.CustomizableUI.addWidgetToArea("save-page-button", "nav-bar");
  newWin.CustomizableUI.addWidgetToArea("open-file-button", "nav-bar");

  let savePageButton = newWin.document.getElementById("save-page-button");
  let openFileButton = newWin.document.getElementById("open-file-button");

  Assert.equal(
    savePageButton.getAttribute("disabled"),
    "true",
    "save-page-button is disabled"
  );
  Assert.equal(
    openFileButton.getAttribute("disabled"),
    "true",
    "open-file-button is disabled"
  );

  newWin.CustomizableUI.reset();
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_context_menu_items_disabled() {
  await BrowserTestUtils.withNewTab("https://example.org/", async browser => {
    let contextMenu = document.getElementById("contentAreaContextMenu");
    let promiseContextMenuOpen = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );

    await BrowserTestUtils.synthesizeMouse(
      "a",
      0,
      0,
      {
        type: "contextmenu",
        button: 2,
        centered: true,
      },
      browser
    );

    await promiseContextMenuOpen;

    for (let item of [
      "context-saveimage",
      "context-savepage",
      "context-savelink",
      "context-savevideo",
      "context-saveaudio",
      "context-video-saveimage",
      "context-saveaudio",
    ]) {
      Assert.equal(
        document.getElementById(item).disabled,
        true,
        `${item} item is disabled`
      );
    }

    contextMenu.hidePopup();
  });
});

add_task(async function test_notification() {
  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see changes from the policy
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  await BrowserTestUtils.withNewTab(
    { gBrowser: newWin.gBrowser, url: "https://example.org/" },
    async browser => {
      await SpecialPowers.spawn(browser, [], async function () {
        let elem = content.document.createElement("input");
        elem.setAttribute("type", "file");

        content.document.notifyUserGestureActivation();
        elem.showPicker();
      });

      let notificationBox = browser.getTabBrowser().getNotificationBox(browser);

      let notification = await TestUtils.waitForCondition(() =>
        notificationBox.getNotificationWithValue("filepicker-blocked")
      );

      Assert.ok(
        notification,
        "filepicker-blocked notification appears when showPicker is called"
      );
    }
  );
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_cancel_event() {
  await BrowserTestUtils.withNewTab("https://example.org/", async browser => {
    let eventType = await SpecialPowers.spawn(browser, [], async function () {
      let elem = content.document.createElement("input");
      elem.setAttribute("type", "file");
      let cancelPromise = new Promise(resolve =>
        elem.addEventListener("cancel", resolve, { once: true })
      );
      content.document.notifyUserGestureActivation();
      elem.showPicker();
      let event = await cancelPromise;
      return event.type;
    });

    Assert.equal(
      eventType,
      "cancel",
      "cancel event should be dispatched when showPicker is called"
    );
  });
});

add_task(async function test_nsIFilePicker_open() {
  let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);

  picker.init(window.browsingContext, "", Ci.nsIFilePicker.modeSave);

  let result = await new Promise(resolve => picker.open(res => resolve(res)));

  Assert.equal(
    result,
    Ci.nsIFilePicker.returnCancel,
    "nsIFilePicker.open callback should immediately answer returnCancel"
  );
});
