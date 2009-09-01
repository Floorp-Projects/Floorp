const Cc = Components.classes;
const Ci = Components.interfaces;

function WWObserver(watcher) {
  this._watcher = watcher;
  this._status = "Waiting";
}
WWObserver.prototype = {
  _exit: function(status) {
    var ok = (status == "Success");

    this._status = status;
    this._window = null;
    this._watcher.unregisterNotification(this);
    this._watcher = null;

    if (!ok) {
      throw new Error(this._status);
    }
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "domwindowopened":
        this._window = subject.QueryInterface(Ci.nsIDOMJSWindow);
        if (!this._window) {
          this._exit("Error");
        }
        this._window.setTimeout(function doTimeout(window) {
          window.QueryInterface(Ci.nsIDOMWindowInternal).close();
        }, 2000, this._window);
        break;

      case "domwindowclosed":
        if (subject.QueryInterface(Ci.nsIDOMJSWindow) != this._window) {
          this._exit("Error");
        }
        this._exit("Success");
        break;

      default:
        this._exit("Error");
    }
  },

  get status() {
    return this._status;
  }
};

var observer = null;

function start(standalone) {
  var windowCreator = Cc["@mozilla.org/toolkit/app-startup;1"].
                      getService(Ci.nsIWindowCreator);
  var windowWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                      getService(Ci.nsIWindowWatcher);
  windowWatcher.setWindowCreator(windowCreator);

  observer = new WWObserver(windowWatcher);
  windowWatcher.registerNotification(observer, standalone);

  var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                      getService(Ci.nsIPromptService);
  promptService.alert(null, "Howdy!", "This window will close automatically.");
}

function maybeExit() {
  if (observer.status != "Waiting") {
    do_check_eq(observer.status, "Success");
    do_test_finished();
    return;
  }
  do_timeout(100, "maybeExit();");
}

function run_test() {
  do_test_pending();
  start();
  do_timeout(100, "maybeExit();");
}
