/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the permissionmanager 'added', 'changed', 'deleted', and 'cleared'
// notifications behave as expected.

let test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function continue_test()
{
  do_run_generator(test_generator);
}

function do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  let pm = Services.perms;
  let now = Number(Date.now());
  let permType = "test/expiration-perm";
  let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                    .getService(Ci.nsIScriptSecurityManager)
                    .getNoAppCodebasePrincipal(NetUtil.newURI("http://example.com"));

  let observer = new permission_observer(test_generator, now, permType);
  Services.obs.addObserver(observer, "perm-changed", false);

  // Add a permission, to test the 'add' notification. Note that we use
  // do_execute_soon() so that we can use our generator to continue the test
  // where we left off.
  do_execute_soon(function() {
    pm.addFromPrincipal(principal, permType, pm.ALLOW_ACTION, pm.EXPIRE_TIME, now + 100000);
  });
  yield;

  // Alter a permission, to test the 'changed' notification.
  do_execute_soon(function() {
    pm.addFromPrincipal(principal, permType, pm.ALLOW_ACTION, pm.EXPIRE_TIME, now + 200000);
  });
  yield;

  // Remove a permission, to test the 'deleted' notification.
  do_execute_soon(function() {
    pm.removeFromPrincipal(principal, permType);
  });
  yield;

  // Clear permissions, to test the 'cleared' notification.
  do_execute_soon(function() {
    pm.removeAll();
  });
  yield;

  Services.obs.removeObserver(observer, "perm-changed");
  do_check_eq(observer.adds, 1);
  do_check_eq(observer.changes, 1);
  do_check_eq(observer.deletes, 1);
  do_check_true(observer.cleared);

  do_finish_generator_test(test_generator);
}

function permission_observer(generator, now, type) {
  // Set up our observer object.
  this.generator = generator;
  this.pm = Services.perms;
  this.now = now;
  this.type = type;
  this.adds = 0;
  this.changes = 0;
  this.deletes = 0;
  this.cleared = false;
}

permission_observer.prototype = {
  observe: function(subject, topic, data) {
    do_check_eq(topic, "perm-changed");

    // "deleted" means a permission was deleted. aPermission is the deleted permission.
    // "added"   means a permission was added. aPermission is the added permission.
    // "changed" means a permission was altered. aPermission is the new permission.
    // "cleared" means the entire permission list was cleared. aPermission is null.
    if (data == "added") {
      var perm = subject.QueryInterface(Ci.nsIPermission);
      this.adds++;
      switch (this.adds) {
        case 1:
          do_check_eq(this.type, perm.type);
          do_check_eq(this.pm.EXPIRE_TIME, perm.expireType);
          do_check_eq(this.now + 100000, perm.expireTime);
          break;
        default:
          do_throw("too many add notifications posted.");
      }

    } else if (data == "changed") {
      let perm = subject.QueryInterface(Ci.nsIPermission);
      this.changes++;
      switch (this.changes) {
        case 1:
          do_check_eq(this.type, perm.type);
          do_check_eq(this.pm.EXPIRE_TIME, perm.expireType);
          do_check_eq(this.now + 200000, perm.expireTime);
          break;
        default:
          do_throw("too many change notifications posted.");
      }

    } else if (data == "deleted") {
      var perm = subject.QueryInterface(Ci.nsIPermission);
      this.deletes++;
      switch (this.deletes) {
        case 1:
          do_check_eq(this.type, perm.type);
          break;
        default:
          do_throw("too many delete notifications posted.");
      }

    } else if (data == "cleared") {
      // only clear once: at the end
      do_check_false(this.cleared);
      do_check_eq(do_count_enumerator(Services.perms.enumerator), 0);
      this.cleared = true;

    } else {
      do_throw("unexpected data '" + data + "'!");
    }

    // Continue the test.
    do_run_generator(this.generator);
  },
};

