const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

var gIoService = Components.classes["@mozilla.org/network/io-service;1"]
                           .getService(Components.interfaces.nsIIOService);

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function getPrincipalForURI(aURI) {
  var uri =  gIoService.newURI(aURI, null, null);
  return Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                   .getService(Ci.nsIScriptSecurityManager)
                   .getNoAppCodebasePrincipal(uri);
}

function run_test() {
  if (!isParentProcess()) {
    const Ci = Components.interfaces;
    const Cc = Components.classes;

    var mM = Cc["@mozilla.org/childprocessmessagemanager;1"].
                         getService(Ci.nsISyncMessageSender);

    var messageListener = {
      receiveMessage: function(aMessage) {
        switch(aMessage.name) {
          case "TESTING:Stage2A":
            // Permissions created after the child is present
            do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://mozilla.org"), "cookie1"), pm.ALLOW_ACTION);
            do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://mozilla.com"), "cookie2"), pm.DENY_ACTION);
            do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://mozilla.net"), "cookie3"), pm.ALLOW_ACTION);
            do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://firefox.org"), "cookie1"), pm.ALLOW_ACTION);
            do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://firefox.com"), "cookie2"), pm.DENY_ACTION);
            do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://firefox.net"), "cookie3"), pm.ALLOW_ACTION);

            mM.sendAsyncMessage("TESTING:Stage3");
            break;

        }
        return true;
      },
    };

    mM.addMessageListener("TESTING:Stage2A", messageListener);

    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
    do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://mozilla.org"), "cookie1"), pm.ALLOW_ACTION);
    do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://mozilla.com"), "cookie2"), pm.DENY_ACTION);
    do_check_eq(pm.testPermissionFromPrincipal(getPrincipalForURI("http://mozilla.net"), "cookie3"), pm.ALLOW_ACTION);

    mM.sendAsyncMessage("TESTING:Stage2");
  }
}

