/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
