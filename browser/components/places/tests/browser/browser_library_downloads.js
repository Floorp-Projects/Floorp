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
 * The Original Code is Places test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mehdi Mulani <mmulani@mozilla.com> (original author)
 *   Jared Wein <jwein@mozilla.com>
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

/*
 * Tests bug 564900: Add folder specifically for downloads to Library left pane.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=564900
 * This test visits various pages then opens the Library and ensures
 * that both the Downloads folder shows up and that the correct visits
 * are shown in it.
 */

let now = Date.now();

function test() {
  waitForExplicitFinish();

  function onLibraryReady(win) {
    // Add visits to compare contents with.
    fastAddVisit("http://mozilla.com",
                  PlacesUtils.history.TRANSITION_TYPED);
    fastAddVisit("http://google.com",
                  PlacesUtils.history.TRANSITION_DOWNLOAD);
    fastAddVisit("http://en.wikipedia.org",
                  PlacesUtils.history.TRANSITION_TYPED);
    fastAddVisit("http://ubuntu.org",
                  PlacesUtils.history.TRANSITION_DOWNLOAD);

    // Make sure Downloads is present.
    isnot(win.PlacesOrganizer._places.selectedNode, null,
          "Downloads is present and selected");

    // Make sure content in right pane exists.
    let tree = win.document.getElementById("placeContent");
    isnot(tree, null, "placeContent tree exists");

    // Check results.
    var contentRoot = tree.result.root;
    var len = contentRoot.childCount;
    var testUris = ["http://ubuntu.org/", "http://google.com/"];
    for (var i = 0; i < len; i++) {
      is(contentRoot.getChild(i).uri, testUris[i],
          "Comparing downloads shown at index " + i);
    }

    win.close();
    waitForClearHistory(finish);
  }

  openLibrary(onLibraryReady, "Downloads");
}

function fastAddVisit(uri, transition) {
  PlacesUtils.history.addVisit(PlacesUtils._uri(uri), now++ * 1000,
                               null, transition, false, 0);
}
