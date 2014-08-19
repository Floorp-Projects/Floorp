/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gMozLoopAPI;

function promiseGetMozLoopAPI() {
  let deferred = Promise.defer();
  let loopPanel = document.getElementById("loop-notification-panel");
  let btn = document.getElementById("loop-call-button");

  // Wait for the popup to be shown, then we can get the iframe and
  // wait for the iframe's load to be completed.
  loopPanel.addEventListener("popupshown", function onpopupshown() {
    loopPanel.removeEventListener("popupshown", onpopupshown, true);
    let iframe = document.getElementById(btn.getAttribute("notificationFrameId"));

    if (iframe.contentDocument &&
        iframe.contentDocument.readyState == "complete") {
      gMozLoopAPI = iframe.contentWindow.navigator.wrappedJSObject.mozLoop;

      deferred.resolve();
    } else {
      iframe.addEventListener("load", function panelOnLoad(e) {
        iframe.removeEventListener("load", panelOnLoad, true);

        gMozLoopAPI = iframe.contentWindow.navigator.wrappedJSObject.mozLoop;

        // We do this in an execute soon to allow any other event listeners to
        // be handled, just in case.
        deferred.resolve();
      }, true);
    }
  }, true);

  // Now we're setup, click the button.
  btn.click();

  // Remove the iframe after each test. This also avoids mochitest complaining
  // about leaks on shutdown as we intentionally hold the iframe open for the
  // life of the application.
  registerCleanupFunction(function() {
    loopPanel.hidePopup();
    loopPanel.removeChild(document.getElementById(btn.getAttribute("notificationFrameId")));
  });

  return deferred.promise;
}

/**
 * Loads the loop panel by clicking the button and waits for its open to complete.
 * It also registers
 *
 * This assumes that the tests are running in a generatorTest.
 */
function loadLoopPanel() {
  // Set prefs to ensure we don't access the network externally.
  Services.prefs.setCharPref("services.push.serverURL", "ws://localhost/");
  Services.prefs.setCharPref("loop.server", "http://localhost/");

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("services.push.serverURL");
    Services.prefs.clearUserPref("loop.server");
  });

  // Turn off animations to make tests quicker.
  let loopPanel = document.getElementById("loop-notification-panel");
  loopPanel.setAttribute("animate", "false");

  // Now get the actual API.
  yield promiseGetMozLoopAPI();
}

function promiseOAuthParamsSetup(baseURL, params) {
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("POST", baseURL + "/setup_params", true);
  xhr.setRequestHeader("X-Params", JSON.stringify(params));
  xhr.addEventListener("load", () => deferred.resolve(xhr));
  xhr.addEventListener("error", error => deferred.reject(error));
  xhr.send();

  return deferred.promise;
}

function promiseDeletedOAuthParams(baseURL) {
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("DELETE", baseURL + "/setup_params", true);
  xhr.addEventListener("load", () => deferred.resolve(xhr));
  xhr.addEventListener("error", error => deferred.reject(error));
  xhr.send();

  return deferred.promise;
}
