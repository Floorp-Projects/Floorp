// This listens for the next opened window and checks it is of the right url.
// opencallback is called when the new window is fully loaded
// closecallback is called when the window is closed
function WindowOpenListener(url, opencallback, closecallback) {
  this.url = url;
  this.opencallback = opencallback;
  this.closecallback = closecallback;

  Services.wm.addListener(this);
}

WindowOpenListener.prototype = {
  url: null,
  opencallback: null,
  closecallback: null,
  window: null,
  domwindow: null,

  handleEvent: function(event) {
    is(this.domwindow.document.location.href, this.url, "Should have opened the correct window");

    this.domwindow.removeEventListener("load", this, false);
    // Allow any other load handlers to execute
    var self = this;
    executeSoon(function() { self.opencallback(self.domwindow); } );
  },

  onWindowTitleChange: function(window, title) {
  },

  onOpenWindow: function(window) {
    if (this.window)
      return;

    this.window = window;
    this.domwindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindow);
    this.domwindow.addEventListener("load", this, false);
  },

  onCloseWindow: function(window) {
    if (this.window != window)
      return;

    Services.wm.removeListener(this);
    this.opencallback = null;
    this.window = null;
    this.domwindow = null;

    // Let the window close complete
    executeSoon(this.closecallback);
    this.closecallback = null;
  }
};

function test() {
  ok(Application, "Check global access to Application");
  
  // I'd test these against a specific value, but that is bound to flucuate
  ok(Application.id, "Check to see if an ID exists for the Application");
  ok(Application.name, "Check to see if a name exists for the Application");
  ok(Application.version, "Check to see if a version exists for the Application");
  
  var wMediator = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
  var console = wMediator.getMostRecentWindow("global:console");
  waitForExplicitFinish();
  ok(!console, "Console should not already be open");

  new WindowOpenListener("chrome://global/content/console.xul", consoleOpened, consoleClosed);
  Application.console.open();
}

function consoleOpened(win) {
  win.close();
}

function consoleClosed() {
  finish();
}
