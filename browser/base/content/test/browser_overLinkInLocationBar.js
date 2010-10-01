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

var gTestIter;

// TESTS //////////////////////////////////////////////////////////////////////

function smokeTestGenerator() {
  if (ensureOverLinkHidden())
    yield;

  setOverLink("http://example.com/");
  yield;
  checkURLBar(true);

  setOverLink("");
  yield;
  checkURLBar(false);
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
 * Sets the over-link.  This assumes that aStr will cause the over-link to fade
 * in or out.  When its transition has finished, the test iterator is
 * incremented, so you should yield after calling.
 *
 * @param aStr
 *        The over-link will be set to this string or cleared if this is falsey.
 */
function setOverLink(aStr) {
  let overLink = gURLBar._overLinkBox;
  overLink.addEventListener("transitionend", function onTrans(event) {
    if (event.target == overLink && event.propertyName == "opacity") {
      overLink.removeEventListener("transitionend", onTrans, false);
      cont();
    }
  }, false);
  gURLBar.setOverLink(aStr);
}

/**
 * If the over-link is hidden, returns false.  Otherwise, hides the overlink and
 * returns true.  When the overlink is hidden, the test iterator is incremented,
 * so if this function returns true, you should yield after calling.
 *
 * @return True if you should yield and calling and false if not.
 */
function ensureOverLinkHidden() {
  let overLink = gURLBar._overLinkBox;
  if (window.getComputedStyle(overLink, null).opacity == 0)
    return false;

  setOverLink("");
  return true;
}
