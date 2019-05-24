/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the permissionmanager 'added', 'changed', 'deleted', and 'cleared'
// notifications behave as expected.

var test_generator = do_run_test();

function run_test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");
  do_test_pending();
  test_generator.next();
}

function continue_test()
{
  do_run_generator(test_generator);
}

function* do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  let pm = Services.perms;
  let now = Number(Date.now());
  let permType = "test/expiration-perm";
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
              .getService(Ci.nsIScriptSecurityManager);
  let uri = NetUtil.newURI("http://example.com");
  let principal = ssm.createCodebasePrincipal(uri, {});

  let observer = new permission_observer(test_generator, now, permType);
  Services.obs.addObserver(observer, "perm-changed");

  // Add a permission, to test the 'add' notification. Note that we use
  // do_execute_soon() so that we can use our generator to continue the test
  // where we left off.
  executeSoon(function() {
    pm.addFromPrincipal(principal, permType, pm.ALLOW_ACTION, pm.EXPIRE_TIME, now + 100000);
  });
  yield;

  // Alter a permission, to test the 'changed' notification.
  executeSoon(function() {
    pm.addFromPrincipal(principal, permType, pm.ALLOW_ACTION, pm.EXPIRE_TIME, now + 200000);
  });
  yield;

  // Remove a permission, to test the 'deleted' notification.
  executeSoon(function() {
    pm.removeFromPrincipal(principal, permType);
  });
  yield;

  // Clear permissions, to test the 'cleared' notification.
  executeSoon(function() {
    pm.removeAll();
  });
  yield;

  Services.obs.removeObserver(observer, "perm-changed");
  Assert.equal(observer.adds, 1);
  Assert.equal(observer.changes, 1);
  Assert.equal(observer.deletes, 1);
  Assert.ok(observer.cleared);

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
  observe(subject, topic, data) {
    Assert.equal(topic, "perm-changed");

    // "deleted" means a permission was deleted. aPermission is the deleted permission.
    // "added"   means a permission was added. aPermission is the added permission.
    // "changed" means a permission was altered. aPermission is the new permission.
    // "cleared" means the entire permission list was cleared. aPermission is null.
    if (data == "added") {
      let perm = subject.QueryInterface(Ci.nsIPermission);
      this.adds++;
      switch (this.adds) {
        case 1:
          Assert.equal(this.type, perm.type);
          Assert.equal(this.pm.EXPIRE_TIME, perm.expireType);
          Assert.equal(this.now + 100000, perm.expireTime);
          break;
        default:
          do_throw("too many add notifications posted.");
      }

    } else if (data == "changed") {
      let perm = subject.QueryInterface(Ci.nsIPermission);
      this.changes++;
      switch (this.changes) {
        case 1:
          Assert.equal(this.type, perm.type);
          Assert.equal(this.pm.EXPIRE_TIME, perm.expireType);
          Assert.equal(this.now + 200000, perm.expireTime);
          break;
        default:
          do_throw("too many change notifications posted.");
      }

    } else if (data == "deleted") {
      let perm = subject.QueryInterface(Ci.nsIPermission);
      this.deletes++;
      switch (this.deletes) {
        case 1:
          Assert.equal(this.type, perm.type);
          break;
        default:
          do_throw("too many delete notifications posted.");
      }

    } else if (data == "cleared") {
      // only clear once: at the end
      Assert.ok(!this.cleared);
      Assert.equal(do_count_enumerator(Services.perms.enumerator), 0);
      this.cleared = true;

    } else {
      do_throw("unexpected data '" + data + "'!");
    }

    // Continue the test.
    do_run_generator(this.generator);
  },
};
