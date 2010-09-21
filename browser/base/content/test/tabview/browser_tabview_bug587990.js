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
 * The Original Code is bug 587990 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

let newTab;
let win;

function test() {
  waitForExplicitFinish();

  win = window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", "about:blank");

  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);

    newTab = win.gBrowser.addTab();

    let onTabViewFrameInitialized = function() {
      win.removeEventListener(
        "tabviewframeinitialized", onTabViewFrameInitialized, false);

      win.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
      win.TabView.toggle();
    }
    win.addEventListener(
      "tabviewframeinitialized", onTabViewFrameInitialized, false);
    win.TabView.updateContextMenu(
      newTab, win.document.getElementById("context_tabViewMenuPopup"));
  }
  win.addEventListener("load", onLoad, false);
}

function onTabViewWindowLoaded() {
  win.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;

  is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");

  let groupItem = contentWindow.GroupItems.groupItems[0];
  let tabItems = groupItem.getChildren();

  is(tabItems[tabItems.length - 1].tab, newTab, "The new tab exists in the group");

  win.gBrowser.removeTab(newTab);
  whenWindowObservesOnce(win, "domwindowclosed", function() {
    finish();
  });
  win.close();
}

function whenWindowObservesOnce(win, topic, func) {
    let windowWatcher = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
      .getService(Components.interfaces.nsIWindowWatcher);
    let origWin = win;
    let origTopic = topic;
    let origFunc = func;        
    function windowObserver(aSubject, aTopic, aData) {
      let theWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
      if (origWin && theWin != origWin)
        return;
      if(aTopic == origTopic) {
          windowWatcher.unregisterNotification(windowObserver);
          origFunc.apply(this, []);
      }
    }
    windowWatcher.registerNotification(windowObserver);
}
