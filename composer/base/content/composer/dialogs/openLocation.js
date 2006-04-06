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
 * The Original Code is Mozilla Composer.
 *
 * The Initial Developer of the Original Code is
 * Disruptive Innovations SARL.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <daniel.glazman@disruptive-innovations.com>, Original author
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

const nsIFilePicker = Components.interfaces.nsIFilePicker;

var gDialog = {};

function Startup()
{
  if (!window.arguments.length)
    return;

  var type = window.arguments[0];
  gDialog.bundle = document.getElementById("openLocationBundle");
  gDialog.input = document.getElementById("dialog.input");
  gDialog.tabOrWindow = document.getElementById("tabOrWindow");

  gDialog.tabOrWindow.value = type;
  gDialog.prefs = GetPrefs();
}

function onChooseFile()
{
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, gDialog.bundle.getString("chooseFileDialogTitle"), nsIFilePicker.modeOpen);
    
    fp.appendFilters(nsIFilePicker.filterHTML);
    fp.appendFilter(gDialog.bundle.getString("templateFilter"), "*.mzt");
    fp.appendFilters(nsIFilePicker.filterText);
    fp.appendFilters(nsIFilePicker.filterAll);

    if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0)
      gDialog.input.value = fp.fileURL.spec;
  }
  catch(ex) {
  }
}

function OpenFile()
{
  var filename = gDialog.input.value;
  var inTab = (gDialog.tabOrWindow.value == "tab");
  window.opener.OpenFile(filename, inTab);

  if (gDialog.prefs)
  {
    var str = Components.classes["@mozilla.org/supports-string;1"]
                        .createInstance(Components.interfaces.nsISupportsString);
    str.data = filename;
    gDialog.prefs.setComplexValue("general.open_location.last_url",
                         Components.interfaces.nsISupportsString, str);
  }
  // Delay closing slightly to avoid timing bug on Linux.
  window.close();
  return false;
}

