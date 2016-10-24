/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the following bugs:
 *
 * Bug 443745 - View>Sort>of "alpha" sort items is default to Z>A instead of A>Z
 * https://bugzilla.mozilla.org/show_bug.cgi?id=443745
 *
 * Bug 444179 - Library>Views>Sort>Sort by Tags does nothing
 * https://bugzilla.mozilla.org/show_bug.cgi?id=444179
 *
 * Basically, fully tests sorting the placeContent tree in the Places Library
 * window.  Sorting is verified by comparing the nsINavHistoryResult returned by
 * placeContent.result to the expected sort values.
 */

// Two properties of nsINavHistoryResult control the sort of the tree:
// sortingMode and sortingAnnotation.  sortingMode's value is one of the
// nsINavHistoryQueryOptions.SORT_BY_* constants.  sortingAnnotation is the
// annotation used to sort for SORT_BY_ANNOTATION_* mode.
//
// This lookup table maps the possible values of anonid's of the treecols to
// objects that represent the treecols' correct state after the user sorts the
// previously unsorted tree by selecting a column from the Views > Sort menu.
// sortingMode is constructed from the key and dir properties (i.e.,
// SORT_BY_<key>_<dir>) and sortingAnnotation is checked against anno.  anno
// may be undefined if key is not "ANNOTATION".
const SORT_LOOKUP_TABLE = {
  title:        { key: "TITLE",        dir: "ASCENDING"  },
  tags:         { key: "TAGS",         dir: "ASCENDING"  },
  url:          { key: "URI",          dir: "ASCENDING"  },
  date:         { key: "DATE",         dir: "DESCENDING" },
  visitCount:   { key: "VISITCOUNT",   dir: "DESCENDING" },
  dateAdded:    { key: "DATEADDED",    dir: "DESCENDING" },
  lastModified: { key: "LASTMODIFIED", dir: "DESCENDING" },
  description:  { key:  "ANNOTATION",
                  dir:  "ASCENDING",
                  anno: "bookmarkProperties/description" }
};

// This is the column that's sorted if one is not specified and the tree is
// currently unsorted.  Set it to a key substring in the name of one of the
// nsINavHistoryQueryOptions.SORT_BY_* constants, e.g., "TITLE", "URI".
// Method ViewMenu.setSortColumn in browser/components/places/content/places.js
// determines this value.
const DEFAULT_SORT_KEY = "TITLE";

// Part of the test is checking that sorts stick, so each time we sort we need
// to remember it.
var prevSortDir = null;
var prevSortKey = null;

// /////////////////////////////////////////////////////////////////////////////

/**
 * Ensures that the sort of aTree is aSortingMode and aSortingAnno.
 *
 * @param aTree
 *        the tree to check
 * @param aSortingMode
 *        one of the Ci.nsINavHistoryQueryOptions.SORT_BY_* constants
 * @param aSortingAnno
 *        checked only if sorting mode is one of the
 *        Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_* constants
 */
function checkSort(aTree, aSortingMode, aSortingAnno) {
  // The placeContent tree's sort is determined by the nsINavHistoryResult it
  // stores.  Get it and check that the sort is what the caller expects.
  let res = aTree.result;
  isnot(res, null,
        "sanity check: placeContent.result should not return null");

  // Check sortingMode.
  is(res.sortingMode, aSortingMode,
     "column should now have sortingMode " + aSortingMode);

  // Check sortingAnnotation, but only if sortingMode is ANNOTATION.
  if ([Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_ASCENDING,
       Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_DESCENDING].
      indexOf(aSortingMode) >= 0) {
    is(res.sortingAnnotation, aSortingAnno,
       "column should now have sorting annotation " + aSortingAnno);
  }
}

/**
 * Sets the sort of aTree.
 *
 * @param aOrganizerWin
 *        the Places window
 * @param aTree
 *        the tree to sort
 * @param aUnsortFirst
 *        true if the sort should be set to SORT_BY_NONE before sorting by aCol
 *        and aDir
 * @param aShouldFail
 *        true if setSortColumn should fail on aCol or aDir
 * @param aCol
 *        the column of aTree by which to sort
 * @param aDir
 *        either "ascending" or "descending"
 */
function setSort(aOrganizerWin, aTree, aUnsortFirst, aShouldFail, aCol, aDir) {
  if (aUnsortFirst) {
    aOrganizerWin.ViewMenu.setSortColumn();
    checkSort(aTree, Ci.nsINavHistoryQueryOptions.SORT_BY_NONE, "");

    // Remember the sort key and direction.
    prevSortKey = null;
    prevSortDir = null;
  }

  let failed = false;
  try {
    aOrganizerWin.ViewMenu.setSortColumn(aCol, aDir);

    // Remember the sort key and direction.
    if (!aCol && !aDir) {
      prevSortKey = null;
      prevSortDir = null;
    }
    else {
      if (aCol)
        prevSortKey = SORT_LOOKUP_TABLE[aCol.getAttribute("anonid")].key;
      else if (prevSortKey === null)
        prevSortKey = DEFAULT_SORT_KEY;

      if (aDir)
        prevSortDir = aDir.toUpperCase();
      else if (prevSortDir === null)
        prevSortDir = SORT_LOOKUP_TABLE[aCol.getAttribute("anonid")].dir;
    }
  } catch (exc) {
    failed = true;
  }

  is(failed, !!aShouldFail,
     "setSortColumn on column " +
     (aCol ? aCol.getAttribute("anonid") : "(no column)") +
     " with direction " + (aDir || "(no direction)") +
     " and table previously " + (aUnsortFirst ? "unsorted" : "sorted") +
     " should " + (aShouldFail ? "" : "not ") + "fail");
}

/**
 * Tries sorting by an invalid column and sort direction.
 *
 * @param aOrganizerWin
 *        the Places window
 * @param aPlaceContentTree
 *        the placeContent tree in aOrganizerWin
 */
function testInvalid(aOrganizerWin, aPlaceContentTree) {
  // Invalid column should fail by throwing an exception.
  let bogusCol = document.createElement("treecol");
  bogusCol.setAttribute("anonid", "bogusColumn");
  setSort(aOrganizerWin, aPlaceContentTree, true, true, bogusCol, "ascending");

  // Invalid direction reverts to SORT_BY_NONE.
  setSort(aOrganizerWin, aPlaceContentTree, false, false, null, "bogus dir");
  checkSort(aPlaceContentTree, Ci.nsINavHistoryQueryOptions.SORT_BY_NONE, "");
}

/**
 * Tests sorting aPlaceContentTree by column only and then by both column
 * and direction.
 *
 * @param aOrganizerWin
 *        the Places window
 * @param aPlaceContentTree
 *        the placeContent tree in aOrganizerWin
 * @param aUnsortFirst
 *        true if, before each sort we try, we should sort to SORT_BY_NONE
 */
function testSortByColAndDir(aOrganizerWin, aPlaceContentTree, aUnsortFirst) {
  let cols = aPlaceContentTree.getElementsByTagName("treecol");
  ok(cols.length > 0, "sanity check: placeContent should contain columns");

  for (let i = 0; i < cols.length; i++) {
    let col = cols.item(i);
    ok(col.hasAttribute("anonid"),
       "sanity check: column " + col.id + " should have anonid");

    let colId = col.getAttribute("anonid");
    ok(colId in SORT_LOOKUP_TABLE,
       "sanity check: unexpected placeContent column anonid");

    let sortConst =
      "SORT_BY_" + SORT_LOOKUP_TABLE[colId].key + "_" +
      (aUnsortFirst ? SORT_LOOKUP_TABLE[colId].dir : prevSortDir);
    let expectedSortMode = Ci.nsINavHistoryQueryOptions[sortConst];
    let expectedAnno = SORT_LOOKUP_TABLE[colId].anno || "";

    // Test sorting by only a column.
    setSort(aOrganizerWin, aPlaceContentTree, aUnsortFirst, false, col);
    checkSort(aPlaceContentTree, expectedSortMode, expectedAnno);

    // Test sorting by both a column and a direction.
    ["ascending", "descending"].forEach(function (dir) {
      let sortConst =
        "SORT_BY_" + SORT_LOOKUP_TABLE[colId].key + "_" + dir.toUpperCase();
      let expectedSortMode = Ci.nsINavHistoryQueryOptions[sortConst];
      setSort(aOrganizerWin, aPlaceContentTree, aUnsortFirst, false, col, dir);
      checkSort(aPlaceContentTree, expectedSortMode, expectedAnno);
    });
  }
}

/**
 * Tests sorting aPlaceContentTree by direction only.
 *
 * @param aOrganizerWin
 *        the Places window
 * @param aPlaceContentTree
 *        the placeContent tree in aOrganizerWin
 * @param aUnsortFirst
 *        true if, before each sort we try, we should sort to SORT_BY_NONE
 */
function testSortByDir(aOrganizerWin, aPlaceContentTree, aUnsortFirst) {
  ["ascending", "descending"].forEach(function (dir) {
    let key = (aUnsortFirst ? DEFAULT_SORT_KEY : prevSortKey);
    let sortConst = "SORT_BY_" + key + "_" + dir.toUpperCase();
    let expectedSortMode = Ci.nsINavHistoryQueryOptions[sortConst];
    setSort(aOrganizerWin, aPlaceContentTree, aUnsortFirst, false, null, dir);
    checkSort(aPlaceContentTree, expectedSortMode, "");
  });
}

// /////////////////////////////////////////////////////////////////////////////

function test() {
  waitForExplicitFinish();

  openLibrary(function (win) {
        let tree = win.document.getElementById("placeContent");
        isnot(tree, null, "sanity check: placeContent tree should exist");
        // Run the tests.
        testSortByColAndDir(win, tree, true);
        testSortByColAndDir(win, tree, false);
        testSortByDir(win, tree, true);
        testSortByDir(win, tree, false);
        testInvalid(win, tree);
        // Reset the sort to SORT_BY_NONE.
        setSort(win, tree, false, false);
        // Close the window and finish.
        win.close();
        finish();
  });
}
