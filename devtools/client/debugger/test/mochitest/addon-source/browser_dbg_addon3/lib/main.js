var { Cc, Ci } = require("chrome");
var { once } = require("sdk/system/events");

var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
var observer = {
  observe: function () {
    debugger;
  }
};

once("sdk:loader:destroy", () => observerService.removeObserver(observer, "debuggerAttached"));

observerService.addObserver(observer, "debuggerAttached");
