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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
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

function setColorWell(aPicker)
{
  var colorRef;

  colorRef = aPicker.nextSibling; // colour value is held here
  colorRef.setAttribute( "value", aPicker.color );
}

function getColorFromWellAndSetValue(aPickerId)
{
  var picker;
  var colorRef;
  var color;

  picker       = document.getElementById(aPickerId);
  colorRef     = picker.nextSibling;
  color        = colorRef.getAttribute("value");
  picker.color = color;

  return color;
}     

function Startup()
{
  getColorFromWellAndSetValue("labelColorPicker1");
  getColorFromWellAndSetValue("labelColorPicker2");
  getColorFromWellAndSetValue("labelColorPicker3");
  getColorFromWellAndSetValue("labelColorPicker4");
  getColorFromWellAndSetValue("labelColorPicker5");

  return true;
}                   

/* Function to restore pref values to application defaults */
function restoreColorAndDescriptionToDefaults()
{
  var prefColor;
  var description;
  var pickerColor;
  var dataColor;
  var labelDescription;
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefs = prefService.getDefaultBranch(null);

  /* there are only 5 labels */
	for(var i = 1; i <= 5; i++)
  {
    /* set the default description from prefs */
    description = prefs.getComplexValue("mailnews.labels.description." + i,
                                         Components.interfaces.nsIPrefLocalizedString).data;
    labelDescription = document.getElementById("label" + i + "TextBox");
    labelDescription.value = description;

    /* set the default color from prefs */
    prefColor = prefs.getCharPref("mailnews.labels.color." + i);
    pickerColor = document.getElementById("labelColorPicker" + i);
    pickerColor.color = prefColor;

    // need to call setColorWell() so that the default pref value will be updated
    // in the preferences file.
    setColorWell(pickerColor);
  }
}

