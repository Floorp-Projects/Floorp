/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// setup a profile directory
var dir = do_get_profile();

// initialize the permission manager service
var pm = Cc["@mozilla.org/permissionmanager;1"]
          .getService(Ci.nsIPermissionManager);

var ios = Cc["@mozilla.org/network/io-service;1"]
          .getService(Ci.nsIIOService);
var permURI = ios.newURI("http://example.com", null, null);

var theTime = (new Date()).getTime();

var numadds = 0;
var numchanges = 0;
var numdeletes = 0;
var needsToClear = true;

// will listen for stuff.
var observer = {
  QueryInterface: 
  function(iid) {
    if (iid.equals(Ci.nsISupports) || 
        iid.equals(Ci.nsIObserver))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE; 
  },

  observe:
  function(subject, topic, data) {
    if (topic !== "perm-changed")
      return;

    // "deleted" means a permission was deleted. aPermission is the deleted permission.
    // "added"   means a permission was added. aPermission is the added permission.
    // "changed" means a permission was altered. aPermission is the new permission.
    // "cleared" means the entire permission list was cleared. aPermission is null.
    if (data == "added") {
      var perm = subject.QueryInterface(Ci.nsIPermission);
      numadds++;
      switch (numadds) {
        case 1: /* first add */
          do_check_eq(pm.EXPIRE_TIME, perm.expireType);
          do_check_eq(theTime + 10000, perm.expireTime);
          break;
        case 2: /* second add (permission-notify) */
          do_check_eq(pm.EXPIRE_NEVER, perm.expireType);
          do_check_eq(pm.DENY_ACTION, perm.capability);
          break;
        default:
          do_throw("too many add notifications posted.");
      }
      do_test_finished();

    } else if (data == "changed") {
      var perm = subject.QueryInterface(Ci.nsIPermission);
      numchanges++;
      switch (numchanges) {
        case 1:
          do_check_eq(pm.EXPIRE_TIME, perm.expireType);
          do_check_eq(theTime + 20000, perm.expireTime);
          break;
        default:
          do_throw("too many change notifications posted.");
      }
      do_test_finished();

    } else if (data == "deleted") {
      var perm = subject.QueryInterface(Ci.nsIPermission);
      numdeletes++;
      switch (numdeletes) {
        case 1:
          do_check_eq("test/permission-notify", perm.type);
          break;
        default:
          do_throw("too many delete notifications posted.");
      }
      do_test_finished();

    } else if (data == "cleared") {
      // only clear once: at the end
      do_check_true(needsToClear);
      needsToClear = false;
      do_test_finished();
    } else {
      dump("subject: " + subject + "  data: " + data + "\n");
    }
  },
};

function run_test() {

  var obs = Cc["@mozilla.org/observer-service;1"].getService()
            .QueryInterface(Ci.nsIObserverService);

  obs.addObserver(observer, "perm-changed", false);

  // add a permission
  do_test_pending(); // for 'add' notification
  pm.add(permURI, "test/expiration-perm", pm.ALLOW_ACTION, pm.EXPIRE_TIME, theTime + 10000);

  do_test_pending(); // for 'change' notification
  pm.add(permURI, "test/expiration-perm", pm.ALLOW_ACTION, pm.EXPIRE_TIME, theTime + 20000);

  do_test_pending(); // for 'add' notification
  pm.add(permURI, "test/permission-notify", pm.DENY_ACTION);

  do_test_pending(); // for 'deleted' notification
  pm.remove(permURI.asciiHost, "test/permission-notify");

  do_test_pending(); // for 'cleared' notification
  pm.removeAll();

  do_timeout(100, cleanup);
}

function cleanup() {
  obs.removeObserver(observer, "perm-changed");
}
