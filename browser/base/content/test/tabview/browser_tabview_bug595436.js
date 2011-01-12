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
 * The Original Code is a test for bug 595436.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Tim Taubert <tim.taubert@gmx.de>
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
  
  window.addEventListener('tabviewshown', onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener('tabviewshown', onTabViewWindowLoaded, false);
  
  let contentWindow = document.getElementById('tab-view').contentWindow;
  let search = contentWindow.document.getElementById('search');
  let searchButton = contentWindow.document.getElementById('searchbutton');
  
  let isSearchEnabled = function () {
    return 'none' != search.style.display;
  }
  
  let assertSearchIsEnabled = function () {
    ok(isSearchEnabled(), 'search is enabled');
  }
  
  let assertSearchIsDisabled = function () {
    ok(!isSearchEnabled(), 'search is disabled');
  }
  
  let testSearchInitiatedByKeyPress = function () {
    EventUtils.synthesizeKey('a', {});
    assertSearchIsEnabled();
    
    EventUtils.synthesizeKey('VK_BACK_SPACE', {});
    assertSearchIsDisabled();
  }
  
  let testSearchInitiatedByMouseClick = function () {
    EventUtils.sendMouseEvent({type: 'mousedown'}, searchButton, contentWindow);
    assertSearchIsEnabled();
    
    EventUtils.synthesizeKey('a', {});
    EventUtils.synthesizeKey('VK_BACK_SPACE', {});
    EventUtils.synthesizeKey('VK_BACK_SPACE', {});
    assertSearchIsEnabled();
    
    EventUtils.synthesizeKey('VK_ESCAPE', {});
    assertSearchIsDisabled();
  }
  
  let finishTest = function () {
    let onTabViewHidden = function () {
      window.removeEventListener('tabviewhidden', onTabViewHidden, false);
      finish();
    }
    
    window.addEventListener('tabviewhidden', onTabViewHidden, false);
    TabView.hide();
  }
  
  assertSearchIsDisabled();
  
  testSearchInitiatedByKeyPress();
  testSearchInitiatedByMouseClick();
  
  finishTest();
}
