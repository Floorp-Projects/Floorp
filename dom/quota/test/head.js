/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gActiveListeners = {};

// These event (un)registration handlers only work for one window, DONOT use
// them with multiple windows.

function registerPopupEventHandler(eventName, callback, win)
{
  if (!win) {
    win = window;
  }
  gActiveListeners[eventName] = function (event) {
    if (event.target != win.PopupNotifications.panel)
      return;
    win.PopupNotifications.panel.removeEventListener(
                                                   eventName,
                                                   gActiveListeners[eventName]);
    delete gActiveListeners[eventName];

    callback.call(win.PopupNotifications.panel);
  }
  win.PopupNotifications.panel.addEventListener(eventName,
                                                gActiveListeners[eventName]);
}

function unregisterAllPopupEventHandlers(win)
{
  if (!win) {
    win = window;
  }
  for (let eventName in gActiveListeners) {
    win.PopupNotifications.panel.removeEventListener(
                                                   eventName,
                                                   gActiveListeners[eventName]);
  }
  gActiveListeners = {};
}

function triggerMainCommand(popup, win)
{
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

function triggerSecondaryCommand(popup, win)
{
  if (!win) {
    win = window;
  }
  info("triggering secondary command");
  let notifications = popup.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  let notification = notifications[0];
  EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {}, win);
}

function dismissNotification(popup, win)
{
  if (!win) {
    win = window;
  }
  info("dismissing notification");
  executeSoon(function () {
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  });
}

function waitForMessage(aMessage, browser)
{
  return new Promise((resolve, reject) => {
    function contentScript() {
      addEventListener("message", function(event) {
        sendAsyncMessage("testLocal:persisted",
          {persisted: event.data});
      }, {once: true}, true);
    }

    let script = "data:,(" + contentScript.toString() + ")();";

    let mm = browser.selectedBrowser.messageManager;

    mm.addMessageListener("testLocal:persisted", function listener(msg) {
      mm.removeMessageListener("testLocal:persisted", listener);
      mm.removeDelayedFrameScript(script);
      is(msg.data.persisted, aMessage, "received " + aMessage);
      if (msg.data.persisted == aMessage) {
        resolve();
      } else {
        reject();
      }
    });

    mm.loadFrameScript(script, true);
  });
}

function removePermission(url, permission)
{
  let uri = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService)
                      .newURI(url);
  let ssm = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                      .getService(Ci.nsIScriptSecurityManager);
  let principal = ssm.createCodebasePrincipal(uri, {});

  Components.classes["@mozilla.org/permissionmanager;1"]
            .getService(Components.interfaces.nsIPermissionManager)
            .removeFromPrincipal(principal, permission);
}

function getPermission(url, permission)
{
  let uri = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService)
                      .newURI(url);
  let ssm = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                      .getService(Ci.nsIScriptSecurityManager);
  let principal = ssm.createCodebasePrincipal(uri, {});

  return Components.classes["@mozilla.org/permissionmanager;1"]
                   .getService(Components.interfaces.nsIPermissionManager)
                   .testPermissionFromPrincipal(principal, permission);
}
