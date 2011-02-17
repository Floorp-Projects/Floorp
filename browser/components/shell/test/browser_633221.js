function test() {
  let osString = Cc["@mozilla.org/xre/app-info;1"].
                 getService(Ci.nsIXULRuntime).OS;

  // this test is Linux-specific
  if (osString != "Linux")
    return;

  let shell = Cc["@mozilla.org/browser/shell-service;1"].
              getService(Ci.nsIShellService);

  shell.setDefaultBrowser(true, false);
  ok(shell.isDefaultBrowser(true), "we got here and are the default browser");
}
