/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals log */
/* globals main, deviceInfo, catcher, communication, browser */

"use strict";

this.auth = (function() {
  const exports = {};

  let registrationInfo;
  let initialized = false;
  let authHeader = null;
  let sentryPublicDSN = null;
  let abTests = {};
  let accountId = null;

  const fetchStoredInfo = catcher.watchPromise(
    browser.storage.local.get(["registrationInfo", "abTests"]).then(result => {
      if (result.abTests) {
        abTests = result.abTests;
      }
      if (result.registrationInfo) {
        registrationInfo = result.registrationInfo;
      }
    })
  );

  function generateRegistrationInfo() {
    const info = {
      secret: crypto.randomUUID(),
      registered: false,
    };
    return info;
  }

  function register() {
    return new Promise((resolve, reject) => {
      const registerUrl = main.getBackend() + "/api/register";
      // TODO: replace xhr with Fetch #2261
      const req = new XMLHttpRequest();
      req.open("POST", registerUrl);
      req.setRequestHeader("content-type", "application/json");
      req.onload = catcher.watchFunction(() => {
        if (req.status === 200) {
          log.info("Registered login");
          initialized = true;
          saveAuthInfo(JSON.parse(req.responseText));
          resolve(true);
        } else {
          log.warn("Error in response:", req.responseText);
          const exc = new Error("Bad response: " + req.status);
          exc.popupMessage = "LOGIN_ERROR";
          reject(exc);
        }
      });
      req.onerror = catcher.watchFunction(() => {
        const exc = new Error("Error contacting server");
        exc.popupMessage = "LOGIN_CONNECTION_ERROR";
        reject(exc);
      });
      req.send(
        JSON.stringify({
          secret: registrationInfo.secret,
          deviceInfo: JSON.stringify(deviceInfo()),
        })
      );
    });
  }

  function login(options) {
    const { ownershipCheck, noRegister } = options || {};
    return new Promise((resolve, reject) => {
      return fetchStoredInfo.then(() => {
        if (!registrationInfo) {
          registrationInfo = generateRegistrationInfo();
          log.info("Generating new device authentication ID", registrationInfo);
          browser.storage.local.set({ registrationInfo });
        }
        const loginUrl = main.getBackend() + "/api/login";
        // TODO: replace xhr with Fetch #2261
        const req = new XMLHttpRequest();
        req.open("POST", loginUrl);
        req.onload = catcher.watchFunction(() => {
          if (req.status === 404) {
            if (noRegister) {
              resolve(false);
            } else {
              resolve(register());
            }
          } else if (req.status >= 300) {
            log.warn("Error in response:", req.responseText);
            const exc = new Error("Could not log in: " + req.status);
            exc.popupMessage = "LOGIN_ERROR";
            reject(exc);
          } else if (req.status === 0) {
            const error = new Error("Could not log in, server unavailable");
            error.popupMessage = "LOGIN_CONNECTION_ERROR";
            reject(error);
          } else {
            initialized = true;
            const jsonResponse = JSON.parse(req.responseText);
            log.info("Screenshots logged in");
            saveAuthInfo(jsonResponse);
            if (ownershipCheck) {
              resolve({ isOwner: jsonResponse.isOwner });
            } else {
              resolve(true);
            }
          }
        });
        req.onerror = catcher.watchFunction(() => {
          const exc = new Error("Connection failed");
          exc.url = loginUrl;
          exc.popupMessage = "CONNECTION_ERROR";
          reject(exc);
        });
        req.setRequestHeader("content-type", "application/json");
        req.send(
          JSON.stringify({
            secret: registrationInfo.secret,
            deviceInfo: JSON.stringify(deviceInfo()),
            ownershipCheck,
          })
        );
      });
    });
  }

  function saveAuthInfo(responseJson) {
    accountId = responseJson.accountId;
    if (responseJson.sentryPublicDSN) {
      sentryPublicDSN = responseJson.sentryPublicDSN;
    }
    if (responseJson.authHeader) {
      authHeader = responseJson.authHeader;
      if (!registrationInfo.registered) {
        registrationInfo.registered = true;
        catcher.watchPromise(browser.storage.local.set({ registrationInfo }));
      }
    }
    if (responseJson.abTests) {
      abTests = responseJson.abTests;
      catcher.watchPromise(browser.storage.local.set({ abTests }));
    }
  }

  exports.maybeLogin = function() {
    if (!registrationInfo) {
      return Promise.resolve();
    }

    return exports.authHeaders();
  };

  exports.authHeaders = function() {
    let initPromise = Promise.resolve();
    if (!initialized) {
      initPromise = login();
    }
    return initPromise.then(() => {
      if (authHeader) {
        return { "x-screenshots-auth": authHeader };
      }
      log.warn("No auth header available");
      return {};
    });
  };

  exports.getSentryPublicDSN = function() {
    return sentryPublicDSN;
  };

  exports.getAbTests = function() {
    return abTests;
  };

  exports.isRegistered = function() {
    return registrationInfo && registrationInfo.registered;
  };

  communication.register("getAuthInfo", (sender, ownershipCheck) => {
    return fetchStoredInfo.then(() => {
      // If a device id was never generated, report back accordingly.
      if (!registrationInfo) {
        return null;
      }

      return exports.authHeaders().then(authHeaders => {
        let info = registrationInfo;
        if (info.registered) {
          return login({ ownershipCheck }).then(result => {
            return {
              isOwner: result && result.isOwner,
              accountId,
              authHeaders,
            };
          });
        }
        info = Object.assign({ authHeaders }, info);
        return info;
      });
    });
  });

  return exports;
})();
