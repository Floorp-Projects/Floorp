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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

// This test makes sure that the gPrivateBrowsingUI object, the Private Browsing
// menu item and its XUL <command> element work correctly.

function test() {
  // initialization
  let prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let observer = {
    observe: function (aSubject, aTopic, aData) {
      if (aTopic == "private-browsing")
        this.data = aData;
    },
    data: null
  };
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(observer, "private-browsing", false);
  let pbMenuItem = document.getElementById("privateBrowsingItem");
  // add a new blank tab to ensure the title
  let blankTab = gBrowser.addTab();
  gBrowser.selectedTab = blankTab;
  let originalTitle = document.title;
  let privateBrowsingTitle = document.documentElement.getAttribute("titlemodifier_privatebrowsing");
  waitForExplicitFinish();

  // test the gPrivateBrowsingUI object
  ok(gPrivateBrowsingUI, "The gPrivateBrowsingUI object exists");
  ok(pbMenuItem, "The Private Browsing menu item exists");
  ok(!pbMenuItem.hasAttribute("checked"), "The Private Browsing menu item is not checked initially");
  gPrivateBrowsingUI.toggleMode();
  // check to see if the Private Browsing mode was activated successfully
  is(observer.data, "enter", "Private Browsing mode was activated using the gPrivateBrowsingUI object");
  ok(pbMenuItem.hasAttribute("checked"), "The Private Browsing menu item was correctly checked");
  gPrivateBrowsingUI.toggleMode()
  // check to see if the Private Browsing mode was deactivated successfully
  is(observer.data, "exit", "Private Browsing mode was deactivated using the gPrivateBrowsingUI object");
  ok(!pbMenuItem.hasAttribute("checked"), "The Private Browsing menu item was correctly unchecked");

  // test the menu item
  window.focus();
  let timer = Cc["@mozilla.org/timer;1"].
              createInstance(Ci.nsITimer);
  timer.initWithCallback({
    notify: function(timer) {
      // get the access keys
      let toolsKey = document.getElementById("tools-menu").getAttribute("accesskey");
      let pbKey = pbMenuItem.getAttribute("accesskey");

      // get the access key modifier
      function accessKeyModifier () {
        switch (gPrefService.getIntPref("ui.key.generalAccessKey")) {
        case -1:
          let chromeAccessKey = gPrefService.getIntPref("ui.key.chromeAccess");
          if (chromeAccessKey == 0)
            ok(false, "ui.key.chromeAccess was set to 0, so access keys for chrome are disabled");
          else
            return {
              shiftKey: (chromeAccessKey & 1) != 0,
              accelKey: (chromeAccessKey & 2) != 0,
              altKey: (chromeAccessKey & 4) != 0,
              metaKey: (chromeAccessKey & 8) != 0
            };
          break;

        case Ci.nsIDOMKeyEvent.DOM_VK_SHIFT:
          return { shiftKey: true };
        case Ci.nsIDOMKeyEvent.DOM_VK_CONTROL:
          return { accelKey: true };
        case Ci.nsIDOMKeyEvent.DOM_VK_ALT:
          return { altKey: true };
        case Ci.nsIDOMKeyEvent.DOM_VK_META:
          return { metaKey: true };
        default:
          ok(false, "Invalid value for the ui.key.generalAccessKey pref: " +
            gPrefService.getIntPref("ui.key.generalAccessKey"));
        }
      }

      let popup = document.getElementById("menu_ToolsPopup");
      // activate the Private Browsing menu item
      popup.addEventListener("popupshown", function () {
        this.removeEventListener("popupshown", arguments.callee, false);

        this.addEventListener("popuphidden", function () {
          this.removeEventListener("popuphidden", arguments.callee, false);

          // check to see if the Private Browsing mode was activated successfully
          is(observer.data, "enter", "Private Browsing mode was activated using the menu");
          // check to see that the window title has been changed correctly
          is(document.title, privateBrowsingTitle, "Private browsing mode has correctly changed the title");

          // toggle the Private Browsing menu item
          this.addEventListener("popupshown", function () {
            this.removeEventListener("popupshown", arguments.callee, false);

            this.addEventListener("popuphidden", function () {
              this.removeEventListener("popuphidden", arguments.callee, false);

              // check to see if the Private Browsing mode was deactivated successfully
              is(observer.data, "exit", "Private Browsing mode was deactivated using the menu");
              // check to see that the window title has been restored correctly
              is(document.title, originalTitle, "Private browsing mode has correctly restored the title");

              // now, test using the <command> object
              let cmd = document.getElementById("Tools:PrivateBrowsing");
              isnot(cmd, null, "XUL command object for the private browsing service exists");
              var func = new Function("", cmd.getAttribute("oncommand"));
              func.call(cmd);
              // check to see if the Private Browsing mode was activated successfully
              is(observer.data, "enter", "Private Browsing mode was activated using the command object");
              // check to see that the window title has been changed correctly
              is(document.title, privateBrowsingTitle, "Private browsing mode has correctly changed the title");
              func.call(cmd);
              // check to see if the Private Browsing mode was deactivated successfully
              is(observer.data, "exit", "Private Browsing mode was deactivated using the command object");
              // check to see that the window title has been restored correctly
              is(document.title, originalTitle, "Private browsing mode has correctly restored the title");

              // cleanup
              gBrowser.removeTab(blankTab);
              os.removeObserver(observer, "private-browsing");
              prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
              finish();
            }, false);
            EventUtils.synthesizeKey(pbKey, accessKeyModifier());
          }, false);
          EventUtils.synthesizeKey(toolsKey, accessKeyModifier());
        }, false);
        EventUtils.synthesizeKey(pbKey, accessKeyModifier());
      }, false);
      EventUtils.synthesizeKey(toolsKey, accessKeyModifier());
    }
  }, 2000, Ci.nsITimer.TYPE_ONE_SHOT);
}
