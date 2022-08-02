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

var { ExtensionError, promiseObserved } = ExtensionUtils;

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

    function getTriggeringPrincipalForUrl(url) {
      if (context.checkLoadURL(url, { dontReportErrors: true })) {
        return context.principal;
      }
      let window = context.currentWindow || windowTracker.topWindow;
      // The extension principal cannot directly load about:-URLs except for about:blank, and
      // possibly some other loads such as moz-extension.  Ensure any page set as a home page
      // will load by using a content principal.
      return Services.scriptSecurityManager.createContentPrincipal(
        Services.io.newURI(url),
        {
          privateBrowsingId: PrivateBrowsingUtils.isBrowserPrivate(
            window.gBrowser
          )
            ? 1
            : 0,
        }
      );
    }

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

        create: async function(createData) {
          let needResize =
            createData.left !== null ||
            createData.top !== null ||
            createData.width !== null ||
            createData.height !== null;
          if (createData.incognito && !context.privateBrowsingAllowed) {
            throw new ExtensionError(
              "Extension does not have permission for incognito mode"
            );
          }

          if (needResize) {
            if (createData.state !== null && createData.state != "normal") {
              throw new ExtensionError(
                `"state": "${createData.state}" may not be combined with "left", "top", "width", or "height"`
              );
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

          // Creating a new window allows one single triggering principal for all tabs that
          // are created in the window.  Due to that, if we need a browser principal to load
          // some urls, we fallback to using a content principal like we do in the tabs api.
          // Throws if url is an array and any url can't be loaded by the extension principal.
          let { allowScriptsToClose, principal } = createData;

          if (createData.tabId !== null) {
            if (createData.url !== null) {
              throw new ExtensionError(
                "`tabId` may not be used in conjunction with `url`"
              );
            }

            if (createData.allowScriptsToClose) {
              throw new ExtensionError(
                "`tabId` may not be used in conjunction with `allowScriptsToClose`"
              );
            }

            let tab = tabTracker.getTab(createData.tabId);
            if (!context.canAccessWindow(tab.ownerGlobal)) {
              throw new ExtensionError(`Invalid tab ID: ${createData.tabId}`);
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
              throw new ExtensionError(
                "`incognito` property must match the incognito state of tab"
              );
            }
            createData.incognito = incognito;

            if (
              createData.cookieStoreId &&
              createData.cookieStoreId !==
                getCookieStoreIdForTab(createData, tab)
            ) {
              throw new ExtensionError(
                "`cookieStoreId` must match the tab's cookieStoreId"
              );
            }

            args.appendElement(tab);
          } else if (createData.url !== null) {
            if (Array.isArray(createData.url)) {
              let array = Cc["@mozilla.org/array;1"].createInstance(
                Ci.nsIMutableArray
              );
              for (let url of createData.url.map(u => context.uri.resolve(u))) {
                // We can only provide a single triggering principal when
                // opening a window, so if the extension cannot normally
                // access a url, we fail.  This includes about and moz-ext
                // urls.
                if (!context.checkLoadURL(url, { dontReportErrors: true })) {
                  return Promise.reject({ message: `Illegal URL: ${url}` });
                }
                array.appendElement(mkstr(url));
              }
              args.appendElement(array);
            } else {
              let url = context.uri.resolve(createData.url);
              args.appendElement(mkstr(url));
              principal = getTriggeringPrincipalForUrl(url);
              if (allowScriptsToClose === null) {
                allowScriptsToClose = url.startsWith("moz-extension://");
              }
            }
          } else {
            let url =
              createData.incognito &&
              !PrivateBrowsingUtils.permanentPrivateBrowsing
                ? "about:privatebrowsing"
                : HomePage.get().split("|", 1)[0];
            args.appendElement(mkstr(url));
            principal = getTriggeringPrincipalForUrl(url);
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
                throw new ExtensionError(
                  "`incognito` cannot be used if incognito mode is disabled"
                );
              }
              features.push("private");
            } else {
              features.push("non-private");
            }
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

          const contentLoaded = new Promise(resolve => {
            window.addEventListener(
              "DOMContentLoaded",
              function() {
                if (allowScriptsToClose) {
                  window.gBrowserAllowScriptsToCloseInitialTabs = true;
                }
                resolve();
              },
              { once: true }
            );
          });

          const startupFinished = promiseObserved(
            "browser-delayed-startup-finished",
            win => win == window
          );

          await contentLoaded;
          await startupFinished;

          if (
            [
              "minimized",
              "fullscreen",
              "docked",
              "normal",
              "maximized",
            ].includes(createData.state)
          ) {
            await win.setState(createData.state);
          }

          if (createData.titlePreface !== null) {
            win.setTitlePreface(createData.titlePreface);
          }
          return win.convert({ populate: true });
        },

        update: async function(windowId, updateInfo) {
          if (updateInfo.state !== null && updateInfo.state != "normal") {
            if (
              updateInfo.left !== null ||
              updateInfo.top !== null ||
              updateInfo.width !== null ||
              updateInfo.height !== null
            ) {
              throw new ExtensionError(
                `"state": "${updateInfo.state}" may not be combined with "left", "top", "width", or "height"`
              );
            }
          }

          let win = windowManager.get(windowId, context);
          if (!win) {
            throw new ExtensionError(`Invalid window ID: ${windowId}`);
          }
          if (updateInfo.focused) {
            win.window.focus();
          }

          if (updateInfo.state !== null) {
            await win.setState(updateInfo.state);
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
