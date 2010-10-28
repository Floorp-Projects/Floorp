const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

var gIoService = Components.classes["@mozilla.org/network/io-service;1"]
                           .getService(Components.interfaces.nsIIOService);

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function run_test() {
  if (isParentProcess()) {
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);

    // Permissions created before the child is present
    pm.add(gIoService.newURI("http://mozilla.org", null, null), "cookie1", pm.ALLOW_ACTION, pm.EXPIRE_NEVER, 0);
    pm.add(gIoService.newURI("http://mozilla.com", null, null), "cookie2", pm.DENY_ACTION, pm.EXPIRE_SESSION, 0);
    pm.add(gIoService.newURI("http://mozilla.net", null, null), "cookie3", pm.ALLOW_ACTION, pm.EXPIRE_TIME, Date.now() + 1000*60*60*24);

    var mM = Cc["@mozilla.org/parentprocessmessagemanager;1"].
             getService(Ci.nsIFrameMessageManager);

    var messageListener = {
      receiveMessage: function(aMessage) {
        switch(aMessage.name) {
          case "TESTING:Stage2":
            // Permissions created after the child is present
            pm.add(gIoService.newURI("http://firefox.org", null, null), "cookie1", pm.ALLOW_ACTION, pm.EXPIRE_NEVER, 0);
            pm.add(gIoService.newURI("http://firefox.com", null, null), "cookie2", pm.DENY_ACTION, pm.EXPIRE_SESSION, 0);
            pm.add(gIoService.newURI("http://firefox.net", null, null), "cookie3", pm.ALLOW_ACTION, pm.EXPIRE_TIME, Date.now() + 1000*60*60*24);
            mM.sendAsyncMessage("TESTING:Stage2A");
            break;

          case "TESTING:Stage3":
            do_test_finished();
            break;
        }
        return true;
      },
    };

    mM.addMessageListener("TESTING:Stage2", messageListener);
    mM.addMessageListener("TESTING:Stage3", messageListener);

    do_test_pending();
    do_load_child_test_harness();
    run_test_in_child("test_child.js");
  }
}

