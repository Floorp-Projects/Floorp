const Cc = Components.classes;
const Ci = Components.interfaces;

  // setup a profile directory
  var dir = do_get_profile();

  // initialize the permission manager service
  var pm = Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);

  var ios = Cc["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService);
  var permURI = ios.newURI("http://example.com", null, null);

function run_test() {

  // add a permission with *now* expiration
  pm.add(permURI, "test/expiration-perm-exp", 1, pm.EXPIRE_TIME, (new Date()).getTime());

  // add a permission with future expiration (100 milliseconds)
  pm.add(permURI, "test/expiration-perm-exp2", 1, pm.EXPIRE_TIME, (new Date()).getTime() + 100);

  // add a permission with future expiration (10000 milliseconds)
  pm.add(permURI, "test/expiration-perm-exp3", 1, pm.EXPIRE_TIME, (new Date()).getTime() + 10000);

  // add a permission without expiration
  pm.add(permURI, "test/expiration-perm-nexp", 1, pm.EXPIRE_NEVER, 0);

  // check that the second two haven't expired yet
  do_check_eq(1, pm.testPermission(permURI, "test/expiration-perm-exp3"));
  do_check_eq(1, pm.testPermission(permURI, "test/expiration-perm-nexp"));

  // ... and the first one has
  do_test_pending();
  do_timeout(10, "verifyFirstExpiration();");

  // ... and that the short-term one will
  do_test_pending();
  do_timeout(200, "verifyExpiration();");

  // clean up
  do_test_pending();
  do_timeout(300, "end_test();");
}

function verifyFirstExpiration() { 
  do_check_eq(0, pm.testPermission(permURI, "test/expiration-perm-exp"));
  do_test_finished();
}

function verifyExpiration() { 
  do_check_eq(0, pm.testPermission(permURI, "test/expiration-perm-exp2")); 
  do_test_finished();
}

function end_test() {
  // clean up
  pm.removeAll();
  do_test_finished();
}
