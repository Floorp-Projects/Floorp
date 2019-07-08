/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  // Force tabfocus for all elements on OSX.
  SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] }).then(
    setup
  );
}

// Focusing on notification icon buttons is handled by the ToolbarKeyboardNavigator
// component and arrow keys (see browser/base/content/browser-toolbarKeyNav.js).
function focusNotificationAnchor(anchor) {
  let urlbarContainer = anchor.closest("#urlbar-container");
  urlbarContainer.querySelector("toolbartabstop").focus();
  const identityBox = urlbarContainer.querySelector("#identity-box");
  is(document.activeElement, identityBox, "Identity box is focused.");
  while (document.activeElement !== anchor) {
    EventUtils.synthesizeKey("ArrowRight");
  }
}

var tests = [
  // Test that for persistent notifications,
  // the secondary action is triggered by pressing the escape key.
  {
    id: "Test#1",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.persistent = true;
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      EventUtils.synthesizeKey("KEY_Escape");
    },
    onHidden(popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked");
      ok(this.notifyObj.secondaryActionClicked, "secondaryAction was clicked");
      ok(
        !this.notifyObj.dismissalCallbackTriggered,
        "dismissal callback wasn't triggered"
      );
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
      is(
        this.notifyObj.mainActionSource,
        undefined,
        "shouldn't have a main action source."
      );
      is(
        this.notifyObj.secondaryActionSource,
        "esc-press",
        "secondary action should be from ESC key press"
      );
    },
  },
  // Test that for non-persistent notifications, the escape key dismisses the notification.
  {
    id: "Test#2",
    async run() {
      await waitForWindowReadyForPopupNotifications(window);
      this.notifyObj = new BasicNotification(this.id);
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      EventUtils.synthesizeKey("KEY_Escape");
    },
    onHidden(popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked");
      ok(
        !this.notifyObj.secondaryActionClicked,
        "secondaryAction was not clicked"
      );
      ok(
        this.notifyObj.dismissalCallbackTriggered,
        "dismissal callback triggered"
      );
      ok(
        !this.notifyObj.removedCallbackTriggered,
        "removed callback was not triggered"
      );
      is(
        this.notifyObj.mainActionSource,
        undefined,
        "shouldn't have a main action source."
      );
      is(
        this.notifyObj.secondaryActionSource,
        undefined,
        "shouldn't have a secondary action source."
      );
      this.notification.remove();
    },
  },
  // Test that the space key on an anchor element focuses an active notification
  {
    id: "Test#3",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        persistent: true,
      });
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let anchor = document.getElementById(this.notifyObj.anchorID);
      focusNotificationAnchor(anchor);
      EventUtils.sendString(" ");
      is(document.activeElement, popup.children[0].closebutton);
      this.notification.remove();
    },
    onHidden(popup) {},
  },
  // Test that you can switch between active notifications with the space key
  // and that the notification is focused on selection.
  {
    id: "Test#4",
    async run() {
      let notifyObj1 = new BasicNotification(this.id);
      notifyObj1.id += "_1";
      notifyObj1.anchorID = "default-notification-icon";
      notifyObj1.addOptions({
        hideClose: true,
        checkbox: {
          label: "Test that elements inside the panel can be focused",
        },
        persistent: true,
      });
      let opened = waitForNotificationPanel();
      let notification1 = showNotification(notifyObj1);
      await opened;

      let notifyObj2 = new BasicNotification(this.id);
      notifyObj2.id += "_2";
      notifyObj2.anchorID = "geo-notification-icon";
      notifyObj2.addOptions({
        persistent: true,
      });
      opened = waitForNotificationPanel();
      let notification2 = showNotification(notifyObj2);
      let popup = await opened;

      // Make sure notification 2 is visible
      checkPopup(popup, notifyObj2);

      // Activate the anchor for notification 1 and wait until it's shown.
      let anchor = document.getElementById(notifyObj1.anchorID);
      focusNotificationAnchor(anchor);
      is(document.activeElement, anchor);
      opened = waitForNotificationPanel();
      EventUtils.sendString(" ");
      popup = await opened;
      checkPopup(popup, notifyObj1);

      is(document.activeElement, popup.children[0].checkbox);

      // Activate the anchor for notification 2 and wait until it's shown.
      anchor = document.getElementById(notifyObj2.anchorID);
      focusNotificationAnchor(anchor);
      is(document.activeElement, anchor);
      opened = waitForNotificationPanel();
      EventUtils.sendString(" ");
      popup = await opened;
      checkPopup(popup, notifyObj2);

      is(document.activeElement, popup.children[0].closebutton);

      notification1.remove();
      notification2.remove();
      goNext();
    },
  },
  // Test that passing the autofocus option will focus an opened notification.
  {
    id: "Test#5",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        autofocus: true,
      });
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);

      // Initial focus on open is null because a panel itself
      // can not be focused, next tab focus will be inside the panel.
      is(Services.focus.focusedElement, null);

      EventUtils.synthesizeKey("KEY_Tab");
      is(Services.focus.focusedElement, popup.children[0].closebutton);
      dismissNotification(popup);
    },
    async onHidden() {
      // Focus the urlbar to check that it stays focused.
      gURLBar.focus();

      // Show another notification and make sure it's not autofocused.
      let notifyObj = new BasicNotification(this.id);
      notifyObj.id += "_2";
      notifyObj.anchorID = "default-notification-icon";

      let opened = waitForNotificationPanel();
      let notification = showNotification(notifyObj);
      let popup = await opened;
      checkPopup(popup, notifyObj);

      // Check that the urlbar is still focused.
      is(Services.focus.focusedElement, gURLBar.inputField);

      this.notification.remove();
      notification.remove();
    },
  },
  // Test that focus is not moved out of a content element if autofocus is not set.
  {
    id: "Test#6",
    async run() {
      let id = this.id;
      await BrowserTestUtils.withNewTab(
        "data:text/html,<input id='test-input'/>",
        async function(browser) {
          let notifyObj = new BasicNotification(id);
          await ContentTask.spawn(browser, {}, function() {
            content.document.getElementById("test-input").focus();
          });

          let opened = waitForNotificationPanel();
          let notification = showNotification(notifyObj);
          await opened;

          // Check that the focused element in the chrome window
          // is either the browser in case we're running on e10s
          // or the input field in case of non-e10s.
          if (gMultiProcessBrowser) {
            is(Services.focus.focusedElement, browser);
          } else {
            is(
              Services.focus.focusedElement,
              browser.contentDocument.getElementById("test-input")
            );
          }

          // Check that the input field is still focused inside the browser.
          await ContentTask.spawn(browser, {}, function() {
            is(
              content.document.activeElement,
              content.document.getElementById("test-input")
            );
          });

          notification.remove();
        }
      );
      goNext();
    },
  },
];
