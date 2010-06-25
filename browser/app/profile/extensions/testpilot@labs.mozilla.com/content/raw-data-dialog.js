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
 * The Original Code is Test Pilot.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jono X <jono@mozilla.com>
 *   Raymond Lee <raymond@appcoast.com>
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

Cu.import("resource://testpilot/modules/setup.js");

function showdbcontents() {
  // experimentId is passed in through window.openDialog.  Access it like so:
  var experimentId = window.arguments[0];
  var experiment = TestPilotSetup.getTaskById(experimentId);
  var dataStore = experiment.dataStore;
  var experimentTitle = experiment.title;
  var listbox = document.getElementById("raw-data-listbox");
  var columnNames = dataStore.getHumanReadableColumnNames();
  var propertyNames = dataStore.getPropertyNames();

  // Set title of dialog box to match title of experiment:
  var dialog = document.getElementById("raw-data-dialog");
  var dialogTitle = dialog.getAttribute("title");
  dialogTitle = dialogTitle + ": " + experimentTitle;
  dialog.setAttribute("title", dialogTitle);

  var i,j;
  // Create the listcols and listheaders in the xul listbox to match the human
  // readable column names provided:
  var listcols = document.getElementById("raw-data-listcols");
  var listhead = document.getElementById("raw-data-listhead");
  for (j = 0; j < columnNames.length; j++) {
    listcols.appendChild(document.createElement("listcol"));
    var newHeader = document.createElement("listheader");
    newHeader.setAttribute("label", columnNames[j]);
    listhead.appendChild(newHeader);
  }

  dataStore.getAllDataAsJSON(true, function(rawData) {
    // Convert each object in the JSON into a row of the listbox.
    for (i = 0; i < rawData.length; i++) {
      var row = document.createElement("listitem");
      for (j = 0; j < columnNames.length; j++) {
        var cell = document.createElement("listcell");
        var value = rawData[i][propertyNames[j]];
        cell.setAttribute("label", value);
        row.appendChild(cell);
      }
      listbox.appendChild(row);
    }
  });
}

// OK button for dialog box.
function onAccept() {
  return true;
}
