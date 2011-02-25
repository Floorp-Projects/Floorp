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
 * The Original Code is a test for bug 618828.
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

  ok(!TabView.isVisible(), 'TabView is hidden');
  let tab = gBrowser.loadOneTab('about:blank#other', {inBackground: true});

  TabView._initFrame(function () {
    newWindowWithTabView(function (win) {
      onTabViewWindowLoaded(win, tab);
    });
  });
}

function onTabViewWindowLoaded(win, tab) {
  let contentWindow = win.TabView.getContentWindow();
  let search = contentWindow.document.getElementById('search');
  let searchbox = contentWindow.document.getElementById('searchbox');
  let searchButton = contentWindow.document.getElementById('searchbutton');
  let results = contentWindow.document.getElementById('results');

  let isSearchEnabled = function () {
    return 'none' != search.style.display;
  }

  let assertSearchIsEnabled = function () {
    ok(isSearchEnabled(), 'search is enabled');
  }

  let assertSearchIsDisabled = function () {
    ok(!isSearchEnabled(), 'search is disabled');
  }

  let enableSearch = function () {
    assertSearchIsDisabled();
    EventUtils.sendMouseEvent({type: 'mousedown'}, searchButton, contentWindow);
  }

  let finishTest = function () {
    win.close();
    gBrowser.removeTab(tab);
    finish();
  }

  let testClickOnSearchBox = function () {
    EventUtils.synthesizeMouseAtCenter(searchbox, {}, contentWindow);
    assertSearchIsEnabled();
  }

  let testClickOnOtherSearchResult = function () {
    // search for the tab from our main window
    searchbox.setAttribute('value', 'other');
    contentWindow.performSearch();

    // prepare to finish when the main window gets focus back
    window.addEventListener('focus', function () {
      window.removeEventListener('focus', arguments.callee, true);
      assertSearchIsDisabled();

      // check that the right tab is active
      is(gBrowser.selectedTab, tab, 'search result is the active tab');

      finishTest();
    }, true);

    // click the first result
    ok(results.firstChild, 'search returns one result');
    EventUtils.synthesizeMouseAtCenter(results.firstChild, {}, contentWindow);
  }

  enableSearch();
  assertSearchIsEnabled();

  testClickOnSearchBox();
  testClickOnOtherSearchResult();
}

// ---------
function newWindowWithTabView(callback) {
  let win = window.openDialog(getBrowserURL(), "_blank", 
                              "chrome,all,dialog=no,height=800,width=800");
  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);
    let onShown = function() {
      win.removeEventListener("tabviewshown", onShown, false);
      callback(win);
    };
    win.addEventListener("tabviewshown", onShown, false);
    win.TabView.toggle();
  }
  win.addEventListener("load", onLoad, false);
}
