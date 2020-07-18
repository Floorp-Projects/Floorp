/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1226498 - Shim Facebook SDK
 *
 * The Facebook SDK is commonly used by sites to allow users to authenticate
 * for logins, but it is blocked by strict tracking protection. It is possible
 * to shim the SDK and allow users to still opt into logging in via Facebook.
 * It is also possible to replace any Facebook widgets or comments with
 * placeholders that the user may click to opt into loading the content.
 */

if (!window.FB) {
  const originalUrl = document.currentScript.src;
  const pendingParses = [];

  function getGUID() {
    return (
      Math.random()
        .toString(36)
        .substring(2) + Date.now().toString(36)
    );
  }

  const shimId = "FacebookSDK";

  const sendMessageToAddon = (function() {
    const pendingMessages = new Map();
    const channel = new MessageChannel();
    channel.port1.onerror = console.error;
    channel.port1.onmessage = event => {
      const { messageId, response } = event.data;
      const resolve = pendingMessages.get(messageId);
      if (resolve) {
        pendingMessages.delete(messageId);
        resolve(response);
      }
    };
    function reconnect() {
      const detail = {
        pendingMessages: [...pendingMessages.values()],
        port: channel.port2,
        shimId,
      };
      window.dispatchEvent(new CustomEvent("ShimConnects", { detail }));
    }
    window.addEventListener("ShimHelperReady", reconnect);
    reconnect();
    return function(message) {
      const messageId = getGUID();
      return new Promise(resolve => {
        const payload = {
          message,
          messageId,
          shimId,
        };
        pendingMessages.set(messageId, resolve);
        channel.port1.postMessage(payload);
      });
    };
  })();

  let ready = false;
  let initInfo;
  const needPopup =
    !/app_runner/.test(window.name) && !/iframe_canvas/.test(window.name);
  const popupName = getGUID();

  if (needPopup) {
    const oldWindowOpen = window.open;
    window.open = function(href, name, params) {
      try {
        const url = new URL(href);
        if (
          url.protocol === "https:" &&
          (url.hostname === "m.facebook.com" ||
            url.hostname === "www.facebook.com") &&
          url.pathname.endsWith("/oauth")
        ) {
          name = popupName;
        }
      } catch (_) {}
      return oldWindowOpen.call(window, href, name, params);
    };
  }

  async function allowFacebookSDK(callback) {
    await sendMessageToAddon("optIn");

    window.FB = undefined;
    const oldInit = window.fbAsyncInit;
    window.fbAsyncInit = () => {
      ready = true;
      if (typeof initInfo !== "undefined") {
        window.FB.init(initInfo);
      } else if (oldInit) {
        oldInit();
      }
      if (callback) {
        callback();
      }
    };

    const s = document.createElement("script");
    s.src = originalUrl;
    await new Promise((resolve, reject) => {
      s.onerror = reject;
      s.onload = function() {
        for (const args of pendingParses) {
          window.FB.XFBML.parse.apply(window.FB.XFBML, args);
        }
        resolve();
      };
      document.head.appendChild(s);
    });
  }

  function buildPopupParams() {
    const { outerWidth, outerHeight, screenX, screenY } = window;
    const { width, height } = window.screen;
    const w = Math.min(width, 400);
    const h = Math.min(height, 400);
    const ua = navigator.userAgent;
    const isMobile = ua.includes("Mobile") || ua.includes("Tablet");
    const left = screenX + (screenX < 0 ? width : 0) + (outerWidth - w) / 2;
    const top = screenY + (screenY < 0 ? height : 0) + (outerHeight - h) / 2.5;
    let params = `left=${left},top=${top},width=${w},height=${h},scrollbars=1,toolbar=0,location=1`;
    if (!isMobile) {
      params = `${params},width=${w},height=${h}`;
    }
    return params;
  }

  async function doLogin(a, b) {
    window.FB.login(a, b);
  }

  function proxy(name, fn) {
    return function() {
      if (ready) {
        return window.FB[name].apply(this, arguments);
      }
      return fn.apply(this, arguments);
    };
  }

  window.FB = {
    api: proxy("api", () => {}),
    AppEvents: {
      EventNames: {},
      logPageView: () => {},
    },
    Event: {
      subscribe: () => {},
    },
    getAccessToken: proxy("getAccessToken", () => null),
    getAuthResponse: proxy("getAuthResponse", () => {
      return { status: "" };
    }),
    getLoginStatus: proxy("getLoginStatus", cb => {
      cb({ status: "" });
    }),
    getUserID: proxy("getUserID", () => {}),
    init: _initInfo => {
      if (ready) {
        doLogin(_initInfo);
      } else {
        initInfo = _initInfo; // in case the site is not using fbAsyncInit
      }
    },
    login: (a, b) => {
      // We have to load Facebook's script, and then wait for it to call
      // window.open. By that time, the popup blocker will likely trigger.
      // So we open a popup now with about:blank, and then make sure FB
      // will re-use that same popup later.
      if (needPopup) {
        window.open("about:blank", popupName, buildPopupParams());
      }
      allowFacebookSDK(() => {
        doLogin(a, b);
      });
    },
    logout: proxy("logout", cb => cb()),
    XFBML: {
      parse: e => {
        pendingParses.push([e]);
      },
    },
  };

  if (window.fbAsyncInit) {
    window.fbAsyncInit();
  }
}
