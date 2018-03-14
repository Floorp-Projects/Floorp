var { Cc, Ci } = require("chrome");
var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);

var observer = {
  observe: function () {
    debugger;
  }
};

observerService.addObserver(observer, "debuggerAttached");

var devtoolsLoaderDestroyObserver = {
  observe: function () {
    // Remove all observers on devtools:loader:destroy
    observerService.removeObserver(observer, "debuggerAttached");
    observerService.removeObserver(devtoolsLoaderDestroyObserver, "devtools:loader:destroy");
  }
};

observerService.addObserver(devtoolsLoaderDestroyObserver, "devtools:loader:destroy");
