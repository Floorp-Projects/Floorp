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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Jan Varga
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

const aliasName = "sqltest";
const complete = Components.interfaces.mozISqlRequest.STATUS_COMPLETE;

var connection;
var result;

var startupObserver = {
  onStartRequest: function(request, ctxt) {
  },
  onStopRequest: function(request, ctxt) {
    if (request.status == complete) {
      result = request.result;
      var ds = result.QueryInterface(Components.interfaces.nsIRDFDataSource);

      var menulist = document.getElementById("statesMenulist");
      menulist.database.AddDataSource(ds);
      menulist.builder.rebuild();
      menulist.selectedIndex = 0;

      var tree = document.getElementById("statesTree");
      tree.database.AddDataSource(ds);
      tree.builder.rebuild();
    }
    else {
      alert(request.errorMessage);
    }
  }
};

var observer = {
  onStartRequest: function(request, ctxt) {
  },
  
  onStopRequest: function(request, ctxt) {
    if (request.status == complete) {
      var element = document.getElementById("asyncStateName");
      if (request.result.rowCount) {
        var enumerator = request.result.enumerate();
        enumerator.first();
        element.value = enumerator.getVariant(0);
      }
      else {
        element.value = "Not found";
      }
    }
    else {
      alert(request.errorMessage);
    }
  }
};

function init() {
  var service = Components.classes["@mozilla.org/sql/service;1"]
                .getService(Components.interfaces.mozISqlService);

  var alias = service.getAlias(aliasName);
  if (!alias) {
    alert("The alias for the sqltest was not defined.");
    return;
  }

  try {
    connection = service.getConnection(alias);
  }
  catch (ex) {
    alert(service.errorMessage);
    return;
  }

  var query = "select code, name from states";
  var request = connection.asyncExecuteQuery(query, null, startupObserver);
}

function syncFindState() {
  var code = document.getElementById("syncStateCode").value
  var query = "select name from states where code = '" + code + "'";
  try {
    var result = connection.executeQuery(query);
    var element = document.getElementById("syncStateName");
    if (result.rowCount) {
      var enumerator = result.enumerate();
      enumerator.first();
      element.value = enumerator.getVariant(0);
    }
    else {
      element.value = "Not found";
    }
  }
  catch (ex) {
    alert(connection.errorMessage);
  }
}

function asyncFindState() {
  var code = document.getElementById("asyncStateCode").value;
  var query = "select name from states where code = '" + code + "'";
  var request = connection.asyncExecuteQuery(query, null, observer);
}

function getSelectedRowIndex() {
  var tree = document.getElementById("statesTree");
  var currentIndex = tree.currentIndex;
  var resource = tree.builderView.getResourceAtIndex(currentIndex);
  var datasource = result.QueryInterface(Components.interfaces.mozISqlDataSource);
  return datasource.getIndexOfResource(resource);
}

function doInsert() {
  window.openDialog("sqltestDialog.xul", "testDialog", "chrome,modal=yes,resizable=no", result);
}

function doUpdate() {
  var rowIndex = this.getSelectedRowIndex();
  window.openDialog("sqltestDialog.xul", "testDialog", "chrome,modal=yes,resizable=no", result, rowIndex);
}

function doDelete() {
  var rowIndex = this.getSelectedRowIndex();
  var enumerator = result.enumerate();
  enumerator.absolute(rowIndex);
  try {
    enumerator.deleteRow();
  }
  catch(ex) {
    alert(enumerator.errorMessage);
  }
}
