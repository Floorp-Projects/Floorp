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
 * The Original Code is browser test code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com>
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
 * This tests the "over-link" that appears in the location bar when the user
 * mouses over a link.  See bug 587908.
 */

const TEST_URL = "http://mochi.test:8888/browser/browser/base/content/test/browser_overLinkInLocationBar.html";

var gTestIter;

// TESTS //////////////////////////////////////////////////////////////////////

function smokeTestGenerator() {
  let tab = openTestPage();
  yield;

  let contentDoc = gBrowser.contentDocument;
  let link = contentDoc.getElementById("link");

  mouseover(link);
  yield;
  checkURLBar(true);

  mouseout(link);
  yield;
  checkURLBar(false);

  gBrowser.removeTab(tab);
}

function test() {
  waitForExplicitFinish();
  gTestIter = smokeTestGenerator();
  cont();
}

// HELPERS ////////////////////////////////////////////////////////////////////

/**
 * Advances the test iterator.  When all iterations have completed, the entire
 * suite is finish()ed.
 */
function cont() {
  try {
    gTestIter.next();
  }
  catch (err if err instanceof StopIteration) {
    finish();
  }
}

/**
 * Asserts that the location bar looks like it should.
 *
 * @param shouldShowOverLink
 *        True if you expect the over-link to be showing and false otherwise.
 */
function checkURLBar(shouldShowOverLink) {
  let overLink = window.getComputedStyle(gURLBar._overLinkBox, null);
  let origin = window.getComputedStyle(gURLBar._originLabel, null);
  let editLayer = window.getComputedStyle(gURLBar._textboxContainer, null);
  if (shouldShowOverLink) {
    isnot(origin.color, "transparent",
          "Origin color in over-link layer should not be transparent");
    is(overLink.opacity, 1, "Over-link should be opaque");
    is(editLayer.color, "transparent",
       "Edit layer color should be transparent");
  }
  else {
    is(origin.color, "transparent",
       "Origin color in over-link layer should be transparent");
    is(overLink.opacity, 0, "Over-link should be transparent");
    isnot(editLayer.color, "transparent",
          "Edit layer color should not be transparent");
  }
}

/**
 * Opens the test URL in a new foreground tab.  When the page has finished
 * loading, the test iterator is advanced, so you should yield after calling.
 *
 * @return The opened <tab>.
 */
function openTestPage() {
  gBrowser.addEventListener("load", function onLoad(event) {
    if (event.target.URL == TEST_URL) {
      gBrowser.removeEventListener("load", onLoad, true);
      cont();
    }
  }, true);
  return gBrowser.loadOneTab(TEST_URL, { inBackground: false });
}

/**
 * Sends a mouseover event to a given anchor node.  When the over-link fade-in
 * transition has completed, the test iterator is advanced, so you should yield
 * after calling.
 *
 * @param anchorNode
 *        An anchor node.
 */
function mouseover(anchorNode) {
  mouseAnchorNode(anchorNode, true);
}

/**
 * Sends a mouseout event to a given anchor node.  When the over-link fade-out
 * transition has completed, the test iterator is advanced, so you should yield
 * after calling.
 *
 * @param anchorNode
 *        An anchor node.
 */
function mouseout(anchorNode) {
  mouseAnchorNode(anchorNode, false);
}

/**
 * Helper for mouseover and mouseout.  Sends a mouse event to a given node.
 * When the over-link fade-in or -out transition has completed, the test
 * iterator is advanced, so you should yield after calling.
 *
 * @param node
 *        An anchor node in a content document.
 * @param over
 *        True for "mouseover" and false for "mouseout".
 */
function mouseAnchorNode(node, over) {
  let overLink = gURLBar._overLinkBox;
  overLink.addEventListener("transitionend", function onTrans(event) {
    if (event.target == overLink) {
      overLink.removeEventListener("transitionend", onTrans, false);
      cont();
    }
  }, false);
  let offset = over ? 0 : -1;
  let eventType = over ? "mouseover" : "mouseout";
  EventUtils.synthesizeMouse(node, offset, offset,
                             { type: eventType, clickCount: 0 },
                             node.ownerDocument.defaultView);
}
