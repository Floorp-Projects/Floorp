/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This test makes sure that private browsing mode disables the "remember"
// option in the cookie accept dialog.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  let cp = Cc["@mozilla.org/embedcomp/cookieprompt-service;1"].
           getService(Ci.nsICookiePromptService);

  waitForExplicitFinish();

  function checkRememberOption(expectedDisabled, callback) {
    let observer = {
      observe: function(aSubject, aTopic, aData) {
        if (aTopic === "domwindowopened") {
          ww.unregisterNotification(this);
          let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
          win.addEventListener("load", function onLoad(event) {
            win.removeEventListener("load", onLoad, false);

            executeSoon(function() {
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
      }
    };
    ww.registerNotification(observer);

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
        const validIIDs = [Components.interfaces.nsISupports,
                           Components.interfaces.nsICookie,
                           Components.interfaces.nsICookie2];
        for (var i = 0; i < validIIDs.length; ++i)
          if (iid == validIIDs[i])
            return this;
        throw Components.results.NS_ERROR_NO_INTERFACE;
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
