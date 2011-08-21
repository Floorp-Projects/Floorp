/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
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

/**
 *  Bug 433231 - Places Library leaks the nsGlobalWindow when closed with a
 *               history entry selected.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=433231
 *
 * STRs: Open Library, select an history entry in History, close Library.
 * ISSUE: We were adding a bookmarks observer when editing a bookmark, when
 *        selecting an history entry the panel was not un-initialized, and
 *        since an histroy entry does not have an itemId, the observer was
 *        never removed.
 */

const TEST_URI = "http://www.mozilla.org/";

function test() {
  function onLibraryReady(organizer) {
    let contentTree = organizer.document.getElementById("placeContent");
    isnot(contentTree, null, "Sanity check: placeContent tree should exist");
    isnot(organizer.PlacesOrganizer, null, "Sanity check: PlacesOrganizer should exist");
    isnot(organizer.gEditItemOverlay, null, "Sanity check: gEditItemOverlay should exist");

    ok(organizer.gEditItemOverlay._initialized, "gEditItemOverlay is initialized");
    isnot(organizer.gEditItemOverlay.itemId, -1, "Editing a bookmark");

    // Select History in the left pane.
    organizer.PlacesOrganizer.selectLeftPaneQuery('History');
    // Select the first history entry.
    let selection = contentTree.view.selection;
    selection.clearSelection();
    selection.rangedSelect(0, 0, true);
    // Check the panel is editing the history entry.
    is(organizer.gEditItemOverlay.itemId, -1, "Editing an history entry");
    // Close Library window.
    organizer.close();
    // Clean up history.
    waitForClearHistory(finish);
  }

  waitForExplicitFinish();
  // Add an history entry.
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");
  PlacesUtils.history.addVisit(PlacesUtils._uri(TEST_URI), Date.now() * 1000,
                               null, PlacesUtils.history.TRANSITION_TYPED,
                               false, 0);

  openLibrary(onLibraryReady);
}
