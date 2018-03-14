var { Cc, Ci } = require("chrome");
var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);

var observer = {
  observe: function () {
    debugger;
  }
};

observerService.addObserver(observer, "debuggerAttached");

var sdkLoaderDestroyObserver = {
  observe: function () {
    // Remove all observers on sdk:loader:destroy
    observerService.removeObserver(observer, "debuggerAttached");
    observerService.removeObserver(sdkLoaderDestroyObserver, "sdk:loader:destroy");
  }
};

observerService.addObserver(sdkLoaderDestroyObserver, "sdk:loader:destroy");
