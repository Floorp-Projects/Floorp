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
 * The Original Code is a test for bug 595521.
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

let fadeAwayUndoButtonDelay;
let fadeAwayUndoButtonDuration;

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", testCloseLastGroup, false);
  TabView.toggle();
}

function testCloseLastGroup() {
  window.removeEventListener("tabviewshown", testCloseLastGroup, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  is(contentWindow.GroupItems.groupItems.length, 1, "Has one group only");

  let groupItem = contentWindow.GroupItems.groupItems[0];
  
  let checkExistence = function() {
    is(contentWindow.GroupItems.groupItems.length, 1, 
       "Still has one group after delay");

    EventUtils.sendMouseEvent(
      { type: "click" }, groupItem.$undoContainer[0], contentWindow);
  };

  groupItem.addSubscriber(groupItem, "groupHidden", function() {
    groupItem.removeSubscriber(groupItem, "groupHidden");
    // it should still stay after 3 ms.
    setTimeout(checkExistence, 3);
  });

  groupItem.addSubscriber(groupItem, "groupShown", function() {
    groupItem.removeSubscriber(groupItem, "groupShown");

    let endGame = function() {
      window.removeEventListener("tabviewhidden", endGame, false);
      ok(!TabView.isVisible(), "Tab View is hidden");

      groupItem.fadeAwayUndoButtonDelay = fadeAwayUndoButtonDelay;
      groupItem.fadeAwayUndoButtonDuration = fadeAwayUndoButtonDuration;

      finish();
    };
    window.addEventListener("tabviewhidden", endGame, false);

    TabView.toggle();
  });

  let closeButton = groupItem.container.getElementsByClassName("close");
  ok(closeButton, "Group item close button exists");

  // store the original values
  fadeAwayUndoButtonDelay = groupItem.fadeAwayUndoButtonDelay;
  fadeAwayUndoButtonDuration = groupItem.fadeAwayUndoButtonDuration;

  // set both fade away delay and duration to 1ms
  groupItem.fadeAwayUndoButtonDelay = 1;
  groupItem.fadeAwayUndoButtonDuration = 1;

  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);
}
