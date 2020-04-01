/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

var { promiseObserved } = ExtensionUtils;

/**
 * An event manager API provider which listens for a DOM event in any browser
 * window, and calls the given listener function whenever an event is received.
 * That listener function receives a `fire` object, which it can use to dispatch
 * events to the extension, and a DOM event object.
 *
 * @param {BaseContext} context
 *        The extension context which the event manager belongs to.
 * @param {string} name
 *        The API name of the event manager, e.g.,"runtime.onMessage".
 * @param {string} event
 *        The name of the DOM event to listen for.
 * @param {function} listener
 *        The listener function to call when a DOM event is received.
 *
 * @returns {object} An injectable api for the new event.
 */
function WindowEventManager(context, name, event, listener) {
  let register = fire => {
    let listener2 = (window, ...args) => {
      if (context.canAccessWindow(window)) {
        listener(fire, window, ...args);
      }
    };

    windowTracker.addListener(event, listener2);
    return () => {
      windowTracker.removeListener(event, listener2);
    };
  };

  return new EventManager({ context, name, register }).api();
}

this.windows = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;

    const { windowManager } = extension;

    return {
      windows: {
        onCreated: WindowEventManager(
          context,
          "windows.onCreated",
          "domwindowopened",
          (fire, window) => {
            fire.async(windowManager.convert(window));
          }
        ),

        onRemoved: WindowEventManager(
          context,
          "windows.onRemoved",
          "domwindowclosed",
          (fire, window) => {
            fire.async(windowTracker.getId(window));
          }
        ),

        onFocusChanged: new EventManager({
          context,
          name: "windows.onFocusChanged",
          register: fire => {
            // Keep track of the last windowId used to fire an onFocusChanged event
            let lastOnFocusChangedWindowId;

            let listener = event => {
              // Wait a tick to avoid firing a superfluous WINDOW_ID_NONE
              // event when switching focus between two Firefox windows.
              Promise.resolve().then(() => {
                let windowId = Window.WINDOW_ID_NONE;
                let window = Services.focus.activeWindow;
                if (window) {
                  if (!context.canAccessWindow(window)) {
                    return;
                  }
                  windowId = windowTracker.getId(window);
                }
                if (windowId !== lastOnFocusChangedWindowId) {
                  fire.async(windowId);
                  lastOnFocusChangedWindowId = windowId;
                }
              });
            };
            windowTracker.addListener("focus", listener);
            windowTracker.addListener("blur", listener);
            return () => {
              windowTracker.removeListener("focus", listener);
              windowTracker.removeListener("blur", listener);
            };
          },
        }).api(),

        get: function(windowId, getInfo) {
          let window = windowTracker.getWindow(windowId, context);
          if (!window || !context.canAccessWindow(window)) {
            return Promise.reject({
              message: `Invalid window ID: ${windowId}`,
            });
          }
          return Promise.resolve(windowManager.convert(window, getInfo));
        },

        getCurrent: function(getInfo) {
          let window = context.currentWindow || windowTracker.topWindow;
          if (!context.canAccessWindow(window)) {
            return Promise.reject({ message: `Invalid window` });
          }
          return Promise.resolve(windowManager.convert(window, getInfo));
        },

        getLastFocused: function(getInfo) {
          let window = windowTracker.topWindow;
          if (!context.canAccessWindow(window)) {
            return Promise.reject({ message: `Invalid window` });
          }
          return Promise.resolve(windowManager.convert(window, getInfo));
        },

        getAll: function(getInfo) {
          let doNotCheckTypes =
            getInfo === null || getInfo.windowTypes === null;
          let windows = [];
          // incognito access is checked in getAll
          for (let win of windowManager.getAll()) {
            if (doNotCheckTypes || getInfo.windowTypes.includes(win.type)) {
              windows.push(win.convert(getInfo));
            }
          }
          return windows;
        },

        create: function(createData) {
          let needResize =
            createData.left !== null ||
            createData.top !== null ||
            createData.width !== null ||
            createData.height !== null;
          if (createData.incognito && !context.privateBrowsingAllowed) {
            return Promise.reject({
              message: "Extension does not have permission for incognito mode",
            });
          }

          if (needResize) {
            if (createData.state !== null && createData.state != "normal") {
              return Promise.reject({
                message: `"state": "${createData.state}" may not be combined with "left", "top", "width", or "height"`,
              });
            }
            createData.state = "normal";
          }

          function mkstr(s) {
            let result = Cc["@mozilla.org/supports-string;1"].createInstance(
              Ci.nsISupportsString
            );
            result.data = s;
            return result;
          }

          let args = Cc["@mozilla.org/array;1"].createInstance(
            Ci.nsIMutableArray
          );

          let principal = context.principal;
          if (createData.tabId !== null) {
            if (createData.url !== null) {
              return Promise.reject({
                message: "`tabId` may not be used in conjunction with `url`",
              });
            }

            if (createData.allowScriptsToClose) {
              return Promise.reject({
                message:
                  "`tabId` may not be used in conjunction with `allowScriptsToClose`",
              });
            }

            let tab = tabTracker.getTab(createData.tabId);
            if (!context.canAccessWindow(tab.ownerGlobal)) {
              return Promise.reject({
                message: `Invalid tab ID: ${createData.tabId}`,
              });
            }
            // Private browsing tabs can only be moved to private browsing
            // windows.
            let incognito = PrivateBrowsingUtils.isBrowserPrivate(
              tab.linkedBrowser
            );
            if (
              createData.incognito !== null &&
              createData.incognito != incognito
            ) {
              return Promise.reject({
                message:
                  "`incognito` property must match the incognito state of tab",
              });
            }
            createData.incognito = incognito;

            if (
              createData.cookieStoreId &&
              createData.cookieStoreId !==
                getCookieStoreIdForTab(createData, tab)
            ) {
              return Promise.reject({
                message: "`cookieStoreId` must match the tab's cookieStoreId",
              });
            }

            args.appendElement(tab);
          } else if (createData.url !== null) {
            if (Array.isArray(createData.url)) {
              let array = Cc["@mozilla.org/array;1"].createInstance(
                Ci.nsIMutableArray
              );
              for (let url of createData.url) {
                array.appendElement(mkstr(url));
              }
              args.appendElement(array);
            } else {
              args.appendElement(mkstr(createData.url));
            }
          } else {
            let url =
              createData.incognito &&
              !PrivateBrowsingUtils.permanentPrivateBrowsing
                ? "about:privatebrowsing"
                : HomePage.get().split("|", 1)[0];
            args.appendElement(mkstr(url));

            if (
              url.startsWith("about:") &&
              !context.checkLoadURL(url, { dontReportErrors: true })
            ) {
              // The extension principal cannot directly load about:-URLs,
              // except for about:blank. So use the system principal instead.
              principal = Services.scriptSecurityManager.getSystemPrincipal();
            }
          }

          args.appendElement(null); // unused
          args.appendElement(null); // referrerInfo
          args.appendElement(null); // postData
          args.appendElement(null); // allowThirdPartyFixup

          if (createData.cookieStoreId) {
            let userContextIdSupports = Cc[
              "@mozilla.org/supports-PRUint32;1"
            ].createInstance(Ci.nsISupportsPRUint32);
            // May throw if validation fails.
            userContextIdSupports.data = getUserContextIdForCookieStoreId(
              extension,
              createData.cookieStoreId,
              createData.incognito
            );
            args.appendElement(userContextIdSupports); // userContextId
          } else {
            args.appendElement(null);
          }

          args.appendElement(context.principal); // originPrincipal - not important.
          args.appendElement(context.principal); // originStoragePrincipal - not important.
          args.appendElement(principal); // triggeringPrincipal
          args.appendElement(
            Cc["@mozilla.org/supports-PRBool;1"].createInstance(
              Ci.nsISupportsPRBool
            )
          ); // allowInheritPrincipal
          // There is no CSP associated with this extension, hence we explicitly pass null as the CSP argument.
          args.appendElement(null); // csp

          let features = ["chrome"];

          if (createData.type === null || createData.type == "normal") {
            features.push("dialog=no", "all");
          } else {
            // All other types create "popup"-type windows by default.
            features.push(
              "dialog",
              "resizable",
              "minimizable",
              "centerscreen",
              "titlebar",
              "close"
            );
          }

          if (createData.incognito !== null) {
            if (createData.incognito) {
              if (!PrivateBrowsingUtils.enabled) {
                return Promise.reject({
                  message:
                    "`incognito` cannot be used if incognito mode is disabled",
                });
              }
              features.push("private");
            } else {
              features.push("non-private");
            }
          }

          let { allowScriptsToClose, url } = createData;
          if (allowScriptsToClose === null) {
            allowScriptsToClose =
              typeof url === "string" && url.startsWith("moz-extension://");
          }

          let window = Services.ww.openWindow(
            null,
            AppConstants.BROWSER_CHROME_URL,
            "_blank",
            features.join(","),
            args
          );

          let win = windowManager.getWrapper(window);
          win.updateGeometry(createData);

          // TODO: focused, type

          return new Promise(resolve => {
            window.addEventListener(
              "DOMContentLoaded",
              function() {
                if (allowScriptsToClose) {
                  window.gBrowserAllowScriptsToCloseInitialTabs = true;
                }
                resolve(
                  promiseObserved(
                    "browser-delayed-startup-finished",
                    win => win == window
                  )
                );
              },
              { once: true }
            );
          }).then(() => {
            if (
              [
                "minimized",
                "fullscreen",
                "docked",
                "normal",
                "maximized",
              ].includes(createData.state)
            ) {
              win.state = createData.state;
            }
            if (createData.titlePreface !== null) {
              win.setTitlePreface(createData.titlePreface);
            }
            return win.convert({ populate: true });
          });
        },

        update: async function(windowId, updateInfo) {
          if (updateInfo.state !== null && updateInfo.state != "normal") {
            if (
              updateInfo.left !== null ||
              updateInfo.top !== null ||
              updateInfo.width !== null ||
              updateInfo.height !== null
            ) {
              return Promise.reject({
                message: `"state": "${updateInfo.state}" may not be combined with "left", "top", "width", or "height"`,
              });
            }
          }

          let win = windowManager.get(windowId, context);
          if (!win) {
            return Promise.reject({
              message: `Invalid window ID: ${windowId}`,
            });
          }
          if (updateInfo.focused) {
            win.window.focus();
          }

          if (updateInfo.state !== null) {
            win.state = updateInfo.state;
          }

          if (updateInfo.drawAttention) {
            // Bug 1257497 - Firefox can't cancel attention actions.
            win.window.getAttention();
          }

          win.updateGeometry(updateInfo);

          if (updateInfo.titlePreface !== null) {
            win.setTitlePreface(updateInfo.titlePreface);
            await win.window.gBrowser.updateTitlebar();
          }

          // TODO: All the other properties, focused=false...

          return win.convert();
        },

        remove: function(windowId) {
          let window = windowTracker.getWindow(windowId, context);
          if (!context.canAccessWindow(window)) {
            return Promise.reject({
              message: `Invalid window ID: ${windowId}`,
            });
          }
          window.close();

          return new Promise(resolve => {
            let listener = () => {
              windowTracker.removeListener("domwindowclosed", listener);
              resolve();
            };
            windowTracker.addListener("domwindowclosed", listener);
          });
        },
      },
    };
  }
};
