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
 * The Original Code is tabview bug598600 test.
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
let newWin;
function test() {
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

  waitForExplicitFinish();

  // open a new window and setup the window state.
  newWin = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  newWin.addEventListener("load", function(event) {
    this.removeEventListener("load", arguments.callee, false);

    let newState = {
      windows: [{
        tabs: [{
          entries: [{ "url": "about:blank" }],
          hidden: true,
          attributes: {},
          extData: {
            "tabview-tab":
              '{"bounds":{"left":20,"top":35,"width":280,"height":210},' +
              '"userSize":null,"url":"about:blank","groupID":1,' + 
              '"imageData":null,"title":null}'
          }
        },{
          entries: [{ url: "about:blank" }],
          index: 1,
          hidden: false,
          attributes: {},
          extData: {
            "tabview-tab": 
              '{"bounds":{"left":375,"top":35,"width":280,"height":210},' + 
              '"userSize":null,"url":"about:blank","groupID":2,' + 
              '"imageData":null,"title":null}'
          }
        }],
        selected:2,
        _closedTabs: [],
        extData: {
          "tabview-groups": '{"nextID":3,"activeGroupId":2}',
          "tabview-group": 
            '{"1":{"bounds":{"left":15,"top":10,"width":320,"height":375},' + 
            '"userSize":null,"locked":{},"title":"","id":1},' + 
            '"2":{"bounds":{"left":380,"top":5,"width":320,"height":375},' + 
            '"userSize":null,"locked":{},"title":"","id":2}}',
          "tabview-ui": '{"pageBounds":{"left":0,"top":0,"width":875,"height":650}}'
        }, sizemode:"normal"
      }]
    };
    ss.setWindowState(newWin, JSON.stringify(newState), true);

    // add a new tab.
    newWin.gBrowser.addTab();
    is(newWin.gBrowser.tabs.length, 3, "There are 3 browser tabs"); 

    let onTabViewShow = function() {
      newWin.removeEventListener("tabviewshown", onTabViewShow, false);

      let contentWindow = newWin.document.getElementById("tab-view").contentWindow;

      is(contentWindow.GroupItems.groupItems.length, 2, "Has two group items");
      is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphan tabs");

      // clean up and finish
      newWin.close();

      finish();
    }
    newWin.addEventListener("tabviewshown", onTabViewShow, false);
    newWin.TabView.toggle();
  }, false);
}
