var ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
var wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);

function browserWindowsCount(expected) {
  var count = 0;
  var e = wm.getEnumerator("navigator:browser");
  while (e.hasMoreElements()) {
    if (!e.getNext().closed)
      ++count;
  }
  is(count, expected,
     "number of open browser windows according to nsIWindowMediator");
  is(JSON.parse(ss.getBrowserState()).windows.length, expected,
     "number of open browser windows according to getBrowserState");
}

function test() {
  waitForExplicitFinish();

  browserWindowsCount(1);

  var win = openDialog(location, "", "chrome,all,dialog=no");
  win.addEventListener("load", function () {
    browserWindowsCount(2);
    win.close();
    browserWindowsCount(1);
    finish();
  }, false);
}
