var sqlService = null;

var name;
var type;
var hostname;
var port;
var database;

function init() {
  sqlService = Components.classes["@mozilla.org/sql/service;1"]
              .getService(Components.interfaces.mozISqlService);

  name = document.getElementById("name");
  type = document.getElementById("type");
  hostname = document.getElementById("hostname");
  port = document.getElementById("port");
  database = document.getElementById("database");

  if (window.arguments) {
    // get original values
    var alias = window.arguments[0];
    sqlService.fetchAlias(alias, name, type, hostname, port, database);
  }
}

function onAccept() {
  if (window.arguments) {
    // update an existing alias
    var alias = window.arguments[0];
    sqlService.updateAlias(alias, name.value, type.value, hostname.value,
                           port.value, database.value);
  }
  else {
    // add a new database
    sqlService.addAlias(name.value, type.value, hostname.value,
                        port.value, database.value);
  }
}
