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

this.windows = class extends ExtensionAPIPersistent {
  windowEventRegistrar(event, listener) {
    let { extension } = this;
    return ({ fire }) => {
      let listener2 = (window, ...args) => {
        if (extension.canAccessWindow(window)) {
          listener(fire, window, ...args);
        }
      };

      windowTracker.addListener(event, listener2);
      return {
        unregister() {
          windowTracker.removeListener(event, listener2);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    };
  }

  PERSISTENT_EVENTS = {
    onCreated: this.windowEventRegistrar("domwindowopened", (fire, window) => {
      fire.async(this.extension.windowManager.convert(window));
    }),
    onRemoved: this.windowEventRegistrar("domwindowclosed", (fire, window) => {
      fire.async(windowTracker.getId(window));
    }),
    onFocusChanged({ fire }) {
      let { extension } = this;
      // Keep track of the last windowId used to fire an onFocusChanged event
      let lastOnFocusChangedWindowId;

      let listener = event => {
        // Wait a tick to avoid firing a superfluous WINDOW_ID_NONE
        // event when switching focus between two Firefox windows.
        Promise.resolve().then(() => {
          let windowId = Window.WINDOW_ID_NONE;
          let window = Services.focus.activeWindow;
          if (window && extension.canAccessWindow(window)) {
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
      return {
        unregister() {
          windowTracker.removeListener("focus", listener);
          windowTracker.removeListener("blur", listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  getAPI(context) {
    let { extension } = context;

    const { windowManager } = extension;

    return {
      windows: {
        onCreated: new EventManager({
          context,
          module: "windows",
          event: "onCreated",
          extensionApi: this,
        }).api(),

        onRemoved: new EventManager({
          context,
          module: "windows",
          event: "onRemoved",
          extensionApi: this,
        }).api(),

        onFocusChanged: new EventManager({
          context,
          module: "windows",
          event: "onFocusChanged",
          extensionApi: this,
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

          args.appendElement(null); // extraOptions
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

        update: function(windowId, updateInfo) {
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
            win.window.gBrowser.updateTitlebar();
          }

          // TODO: All the other properties, focused=false...

          return Promise.resolve(win.convert());
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
