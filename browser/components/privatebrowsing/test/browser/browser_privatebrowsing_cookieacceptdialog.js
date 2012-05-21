/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing mode disables the "remember"
// option in the cookie accept dialog.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let cp = Cc["@mozilla.org/embedcomp/cookieprompt-service;1"].
           getService(Ci.nsICookiePromptService);

  waitForExplicitFinish();

  function checkRememberOption(expectedDisabled, callback) {
    function observer(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;

      Services.ww.unregisterNotification(observer);
      let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function onLoad(event) {
        win.removeEventListener("load", onLoad, false);

        executeSoon(function () {
          let doc = win.document;
          let remember = doc.getElementById("persistDomainAcceptance");
          ok(remember, "The remember checkbox should exist");

          if (expectedDisabled)
            is(remember.getAttribute("disabled"), "true",
               "The checkbox should be disabled");
          else
            ok(!remember.hasAttribute("disabled"),
               "The checkbox should not be disabled");

          win.close();
          callback();
        });
      }, false);
    }
    Services.ww.registerNotification(observer);

    let remember = {};
    const time = (new Date("Jan 1, 2030")).getTime() / 1000;
    let cookie = {
      name: "foo",
      value: "bar",
      isDomain: true,
      host: "mozilla.org",
      path: "/baz",
      isSecure: false,
      expires: time,
      status: 0,
      policy: 0,
      isSession: false,
      expiry: time,
      isHttpOnly: true,
      QueryInterface: function(iid) {
        const validIIDs = [Ci.nsISupports,
                           Ci.nsICookie,
                           Ci.nsICookie2];
        for (var i = 0; i < validIIDs.length; ++i)
          if (iid == validIIDs[i])
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
      }
    };
    cp.cookieDialog(window, cookie, "mozilla.org", 10, false, remember);
  }

  checkRememberOption(false, function() {
    pb.privateBrowsingEnabled = true;
    checkRememberOption(true, function() {
      pb.privateBrowsingEnabled = false;
      checkRememberOption(false, finish);
    });
  });
}
