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
    var uri = window.arguments[0];
    sqlService.getAlias(uri, name, type, hostname, port, database);
  }
}

function onAccept() {
  if (window.arguments) {
    // update an existing alias
    var uri = window.arguments[0];
    sqlService.updateAlias(uri, name.value, type.value, hostname.value,
                           port.value, database.value);
  }
  else {
    // add a new database
    var uri = "urn:aliases:" + name.value;
    sqlService.addAlias(uri, name.value, type.value, hostname.value,
                        port.value, database.value);
  }
}
