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
