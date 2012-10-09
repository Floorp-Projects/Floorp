/*
 * Function created to put a window in PB mode.
 * THIS IS DANGEROUS.  DO NOT DO THIS OUTSIDE OF TESTS!
 */
function setPrivateWindow(aWindow, aEnable) {
  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShellTreeItem)
                 .treeOwner
                 .QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIXULWindow)
                 .docShell.QueryInterface(Ci.nsILoadContext)
                 .usePrivateBrowsing = aEnable;
} 
