/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// The path to the top level directory.
const depth = "../../../../";

var gActiveListeners = {};

loadScript("dom/quota/test/common/browser.js");

function loadScript(path) {
  const url = new URL(depth + path, gTestPath);
  Services.scriptloader.loadSubScript(url.href, this);
}

// These event (un)registration handlers only work for one window, DONOT use
// them with multiple windows.

function registerPopupEventHandler(eventName, callback, win) {
  if (!win) {
    win = window;
  }
  gActiveListeners[eventName] = function(event) {
    if (event.target != win.PopupNotifications.panel) {
      return;
    }
    win.PopupNotifications.panel.removeEventListener(
      eventName,
      gActiveListeners[eventName]
    );
    delete gActiveListeners[eventName];

    callback.call(win.PopupNotifications.panel);
  };
  win.PopupNotifications.panel.addEventListener(
    eventName,
    gActiveListeners[eventName]
  );
}

function unregisterAllPopupEventHandlers(win) {
  if (!win) {
    win = window;
  }
  for (let eventName in gActiveListeners) {
    win.PopupNotifications.panel.removeEventListener(
      eventName,
      gActiveListeners[eventName]
    );
  }
  gActiveListeners = {};
}

function triggerMainCommand(popup, win) {
  if (!win) {
    win = window;
  }
  info("triggering main command");
  let notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  let notification = notifications[0];
  info("triggering command: " + notification.getAttribute("buttonlabel"));

  EventUtils.synthesizeMouseAtCenter(notification.button, {}, win);
}

async function triggerSecondaryCommand(popup, actionIndex, win) {
  if (!win) {
    win = window;
  }

  info("triggering secondary command");
  let notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  let notification = notifications[0];

  if (!actionIndex) {
    await EventUtils.synthesizeMouseAtCenter(
      notification.secondaryButton,
      {},
      win
    );
  } else {
    // Click the dropmarker arrow and wait for the menu to show up.
    let dropdownPromise = BrowserTestUtils.waitForEvent(
      notification.menupopup,
      "popupshown"
    );
    await EventUtils.synthesizeMouseAtCenter(notification.menubutton, {});
    await dropdownPromise;

    let actionMenuItem = notification.querySelectorAll("menuitem")[
      actionIndex - 1
    ];
    await EventUtils.synthesizeMouseAtCenter(actionMenuItem, {});
  }
}

function dismissNotification(popup, win) {
  if (!win) {
    win = window;
  }
  info("dismissing notification");
  executeSoon(function() {
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  });
}

function waitForMessage(aMessage, browser) {
  // We cannot capture aMessage inside the checkFn, so we override the
  // checkFn.toSource to tunnel aMessage instead.
  let checkFn = function() {};
  checkFn.toSource = function() {
    return `function checkFn(event) {
      let message = ${aMessage.toSource()};
      if (event.data == message) {
        return true;
      }
      throw new Error(
       \`Unexpected result: \$\{event.data\}, expected \$\{message\}\`
      );
    }`;
  };

  return BrowserTestUtils.waitForContentEvent(
    browser.selectedBrowser,
    "message",
    /* capture */ true,
    checkFn,
    /* wantsUntrusted */ true
  ).then(() => {
    // An assertion in checkFn wouldn't be recorded as part of the test, so we
    // use this assertion to confirm that we've successfully received the
    // message (we'll only reach this point if that's the case).
    ok(true, "Received message: " + aMessage);
  });
}

function removePermission(url, permission) {
  let uri = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService)
    .newURI(url);
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );
  let principal = ssm.createContentPrincipal(uri, {});

  Cc["@mozilla.org/permissionmanager;1"]
    .getService(Ci.nsIPermissionManager)
    .removeFromPrincipal(principal, permission);
}

function getPermission(url, permission) {
  let uri = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService)
    .newURI(url);
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );
  let principal = ssm.createContentPrincipal(uri, {});

  return Cc["@mozilla.org/permissionmanager;1"]
    .getService(Ci.nsIPermissionManager)
    .testPermissionFromPrincipal(principal, permission);
}
