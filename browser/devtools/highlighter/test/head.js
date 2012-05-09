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
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Rouget <paul@mozilla.com> (Original author)
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

const Cu = Components.utils;
let tempScope = {};
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm", tempScope);
let LayoutHelpers = tempScope.LayoutHelpers;

// Clear preferences that may be set during the course of tests.
function clearUserPrefs()
{
  Services.prefs.clearUserPref("devtools.inspector.htmlPanelOpen");
  Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
}

registerCleanupFunction(clearUserPrefs);

function isHighlighting()
{
  let veil = InspectorUI.highlighter.veilTransparentBox;
  return !(veil.style.visibility == "hidden");
}

function getHighlitNode()
{
  let h = InspectorUI.highlighter;
  if (!isHighlighting() || !h._contentRect)
    return null;

  let a = {
    x: h._contentRect.left,
    y: h._contentRect.top
  };

  let b = {
    x: a.x + h._contentRect.width,
    y: a.y + h._contentRect.height
  };

  // Get midpoint of diagonal line.
  let midpoint = midPoint(a, b);

  return LayoutHelpers.getElementFromPoint(h.win.document, midpoint.x,
    midpoint.y);
}


function midPoint(aPointA, aPointB)
{
  let pointC = { };
  pointC.x = (aPointB.x - aPointA.x) / 2 + aPointA.x;
  pointC.y = (aPointB.y - aPointA.y) / 2 + aPointA.y;
  return pointC;
}

function computedView()
{
  return InspectorUI.sidebar._toolContext("computedview");
}

function computedViewTree()
{
  return computedView().view;
}

function ruleView()
{
  return InspectorUI.sidebar._toolContext("ruleview").view;
}
function synthesizeKeyFromKeyTag(aKeyId) {
  let key = document.getElementById(aKeyId);
  isnot(key, null, "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode"))
    name = key.getAttribute("keycode");
  else if (key.getAttribute("key"))
    name = key.getAttribute("key");

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: modifiersAttr.match("shift"),
    ctrlKey: modifiersAttr.match("ctrl"),
    altKey: modifiersAttr.match("alt"),
    metaKey: modifiersAttr.match("meta"),
    accelKey: modifiersAttr.match("accel")
  }

  EventUtils.synthesizeKey(name, modifiers);
}
