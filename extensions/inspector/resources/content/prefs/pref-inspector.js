/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: ts=2 sw=2 sts=2
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla.org Code.
 *
 * Contributor(s):
 *   Bruno Escherl <aqualon@aquachan.de>
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

function Startup()
{
  SidebarPrefs_initialize();
  enableBlinkPrefs(document.getElementById("inspector.blink.on").value);
}

function enableBlinkPrefs(aTruth)
{
  /* 
   * define the pair of label and control used in the prefpane to allow
   * disabling of both elements, if a pref is locked.
   */
  let els = {
    lbElBorderColor: "cprElBorderColor",
    lbElBorderWidth: "txfElBorderWidth",
    lbElDuration: "txfElDuration",
    lbElSpeed: "txfElSpeed",
    "": "cbElInvert"
  };

  for (let [label, control] in Iterator(els)) {
    let controlElem = document.getElementById(control);

    // only remove disabled attribute, if pref isn't locked
    if (aTruth && !isPrefLocked(controlElem)) {
      controlElem.removeAttribute("disabled");
      if (label)
        document.getElementById(label).removeAttribute("disabled");
    } else {
      controlElem.setAttribute("disabled", true);
      if (label)
        document.getElementById(label).setAttribute("disabled", true);
    }
  }
}

function isPrefLocked(elem)
{
  if (!elem.hasAttribute("preference"))
    return false;

  return document.getElementById(elem.getAttribute("preference")).locked;
}
