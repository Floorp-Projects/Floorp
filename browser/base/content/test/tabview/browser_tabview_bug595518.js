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
 * The Original Code is bug 595518 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 * Ian Gilman <ian@iangilman.com>
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

function test() {
  waitForExplicitFinish();
  
  // show tab view
  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    // verify exit button worked
    ok(!TabView.isVisible(), "Tab View is hidden");
    
    // verify that the exit button no longer has focus
    is(contentWindow.iQ("#exit-button:focus").length, 0, 
       "The exit button doesn't have the focus");

    // verify that the keyboard combo works (this is the crux of bug 595518)
    // Prepare the key combo
    window.addEventListener("tabviewshown", onTabViewShown, false);
    let utils = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                      getInterface(Components.interfaces.nsIDOMWindowUtils);
    let keyCode = 0;
    let charCode;
    let eventObject;
    if (navigator.platform.indexOf("Mac") != -1) {
      charCode = 160; // character code for option (alt) + space
      eventObject = { altKey: true };
    } else {
      charCode = KeyEvent.DOM_VK_SPACE;
      eventObject = { ctrlKey: true };
    }
    
    // Fire off the key combo
    let modifiers = EventUtils._parseModifiers(eventObject);
    let keyDownDefaultHappened = 
       utils.sendKeyEvent("keydown", keyCode, charCode, modifiers);
    utils.sendKeyEvent("keypress", keyCode, charCode, modifiers,
                           !keyDownDefaultHappened);
    utils.sendKeyEvent("keyup", keyCode, charCode, modifiers);
  }
  
  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);
    
    // test if the key combo worked
    ok(TabView.isVisible(), "Tab View is visible");

    // clean up
    let endGame = function() {
      window.removeEventListener("tabviewhidden", endGame, false);

      ok(!TabView.isVisible(), "Tab View is hidden");
      finish();
    }
    window.addEventListener("tabviewhidden", endGame, false);
    TabView.toggle();
  }
  
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // locate exit button
  let button = contentWindow.document.getElementById("exit-button");
  ok(button, "Exit button exists");
  
  // click exit button
  button.focus();
  EventUtils.sendMouseEvent({ type: "click" }, button, contentWindow);
}
