function browserWindowsCount(expected) {
  var count = 0;
  var e = Services.wm.getEnumerator("navigator:browser");
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
  promiseWindowLoaded(win).then(() => {
    browserWindowsCount(2);
    win.close();
    browserWindowsCount(1);
    finish();
  });
}
