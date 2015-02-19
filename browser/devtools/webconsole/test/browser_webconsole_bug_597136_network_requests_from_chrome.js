/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network requests from chrome don't cause the Web Console to
// throw exceptions.

const TEST_URI = "http://example.com/";

let good = true;
let listener = {
    QueryInterface: XPCOMUtils.generateQI([ Ci.nsIObserver ]),
    observe: function(aSubject, aTopic, aData) {
        if (aSubject instanceof Ci.nsIScriptError &&
            aSubject.category === "XPConnect JavaScript" &&
            aSubject.sourceName.contains("webconsole")) {
            good = false;
        }
    }
};

let xhr;

function test() {
    Services.console.registerListener(listener);

    HUDService; // trigger a lazy-load of the HUD Service

    xhr = new XMLHttpRequest();
    xhr.addEventListener("load", xhrComplete, false);
    xhr.open("GET", TEST_URI, true);
    xhr.send(null);
}

function xhrComplete() {
    xhr.removeEventListener("load", xhrComplete, false);
    window.setTimeout(checkForException, 0);
}

function checkForException() {
    ok(good, "no exception was thrown when sending a network request from a " +
       "chrome window");

    Services.console.unregisterListener(listener);
    listener = xhr = null;

    finishTest();
}

