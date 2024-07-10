/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const TEST_URL =
  "data:text/html,<body><div style='width: 100px; height: 100px; background-color: red;'></div></body>";

function waitForEventRemote(aRemote, aEvent) {
  info(`Wait for ${aEvent} event`);
  return SpecialPowers.spawn(aRemote, [aEvent], function (aEvent) {
    return new Promise(resolve => {
      content.document.addEventListener(
        aEvent,
        _e => {
          info(`Received ${aEvent} event`);
          resolve();
        },
        { once: true }
      );
    });
  });
}

function executeSoonRemote(aRemote) {
  return SpecialPowers.spawn(aRemote, [], function () {
    return new Promise(resolve => {
      SpecialPowers.executeSoon(resolve);
    });
  });
}

function requestPointerLockRemote(aRemote) {
  info(`Request pointerlock`);
  return SpecialPowers.spawn(aRemote, [], function () {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    content.document.body.requestPointerLock();
  });
}

function exitPointerLockRemote(aRemote) {
  info(`Exit pointerlock`);
  return SpecialPowers.spawn(aRemote, [], function () {
    return new Promise(resolve => {
      if (!content.document.pointerLockElement) {
        resolve();
        return;
      }

      content.document.addEventListener(
        "pointerlockchange",
        _e => {
          info(`Received pointerlockchange event`);
          resolve();
        },
        { once: true }
      );
      content.document.exitPointerLock();
    });
  });
}

function isPointerLockedRemote(aRemote) {
  return SpecialPowers.spawn(aRemote, [], function () {
    return !!content.document.pointerLockElement;
  });
}

function getPopup(aDocument, aPopupType) {
  let popup = aDocument.getElementById(aPopupType);
  if (!popup) {
    let menuitem = aDocument.createXULElement("menuitem");
    menuitem.label = "This is a test popup";

    popup = aDocument.createXULElement(aPopupType);
    popup.id = aPopupType;
    popup.appendChild(menuitem);
    aDocument.documentElement.appendChild(popup);
  }
  return popup;
}

function testPointerLockPopup(aPopupType, aShouldBlockPointerLock) {
  add_task(async function test_open_menu_popup_when_pointer_is_locked() {
    info(`Test ${aPopupType}`);
    await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
      let promise = waitForEventRemote(browser, "pointerlockchange");
      await requestPointerLockRemote(browser);
      await promise;
      ok(await isPointerLockedRemote(browser), "pointer should be locked");

      if (aShouldBlockPointerLock) {
        promise = waitForEventRemote(browser, "pointerlockchange");
        // Wait for the event listener to get registered on the remote.
        await executeSoonRemote(browser);
      }

      info(`Open popup`);
      let popup = getPopup(document, aPopupType);
      popup.openPopup();

      if (!aShouldBlockPointerLock) {
        promise = executeSoonRemote(browser);
      }

      await promise;
      is(
        await isPointerLockedRemote(browser),
        !aShouldBlockPointerLock,
        "check if pointer is still locked"
      );

      await exitPointerLockRemote(browser);

      info(`Hide popup`);
      popup.hidePopup(true);
    });
  });

  add_task(async function test_request_pointerlock_when_popup_is_opened() {
    info(`Test ${aPopupType}`);
    await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
      info(`Open popup`);
      let popup = getPopup(document, aPopupType);
      popup.openPopup();

      let promise = aShouldBlockPointerLock
        ? waitForEventRemote(browser, "pointerlockerror")
        : waitForEventRemote(browser, "pointerlockchange");
      // Wait for the event listener to get registered on the remote.
      await executeSoonRemote(browser);
      await requestPointerLockRemote(browser);
      await promise;
      is(
        await isPointerLockedRemote(browser),
        !aShouldBlockPointerLock,
        "check if pointer is still locked"
      );

      await exitPointerLockRemote(browser);

      info(`Hide popup`);
      popup.hidePopup(true);
    });
  });
}

testPointerLockPopup("menupopup", true);
testPointerLockPopup("panel", true);
testPointerLockPopup("tooltip", false);
