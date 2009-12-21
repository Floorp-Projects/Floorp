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

// This test makes sure that private browsing mode disables the Block Image
// context menu item.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const TEST_URI = "http://localhost:8888/browser/browser/components/privatebrowsing/test/browser/ctxmenu.html";

  waitForExplicitFinish();

  function checkBlockImageMenuItem(expectedHidden, callback) {
    let tab = gBrowser.addTab();
    gBrowser.selectedTab = tab;
    let browser = gBrowser.getBrowserForTab(tab);
    browser.addEventListener("load", function() {
      browser.removeEventListener("load", arguments.callee, true);

      executeSoon(function() {
        let contextMenu = document.getElementById("contentAreaContextMenu");
        let blockImage = document.getElementById("context-blockimage");
        let image = browser.contentDocument.getElementsByTagName("img")[0];
        ok(image, "The content image should be accessible");

        contextMenu.addEventListener("popupshown", function() {
          contextMenu.removeEventListener("popupshown", arguments.callee, false);

          is(blockImage.hidden, expectedHidden,
             "The Block Image menu item should " + (expectedHidden ? "" : "not ") + "be hidden");
          contextMenu.hidePopup();
          gBrowser.removeTab(tab);
          callback();
        }, false);

        document.popupNode = image;
        EventUtils.synthesizeMouse(image, 2, 2,
                                   {type: "contextmenu", button: 2},
                                   browser.contentWindow);
      });
    }, true);
    browser.loadURI(TEST_URI);
  }

  checkBlockImageMenuItem(false, function() {
    pb.privateBrowsingEnabled = true;
    checkBlockImageMenuItem(true, function() {
      pb.privateBrowsingEnabled = false;
      checkBlockImageMenuItem(false, finish);
    });
  });
}
