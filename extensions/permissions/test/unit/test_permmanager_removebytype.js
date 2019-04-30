/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  // initialize the permission manager service
  let pm = Cc["@mozilla.org/permissionmanager;1"].
        getService(Ci.nsIPermissionManager);

  Assert.equal(perm_count(), 0);

  // add some permissions
  let uri = NetUtil.newURI("http://amazon.com:8080/foobarbaz");
  let uri2 = NetUtil.newURI("http://google.com:2048/quxx");
  let uri3 = NetUtil.newURI("https://google.com/search");

  pm.add(uri, "apple", 3);
  pm.add(uri, "pear", 1);
  pm.add(uri, "cucumber", 1);

  pm.add(uri2, "apple", 2);
  pm.add(uri2, "pear", 2);

  pm.add(uri3, "cucumber", 3);
  pm.add(uri3, "apple", 1);

  Assert.equal(perm_count(), 7);

  pm.removeByType("apple");
  Assert.equal(perm_count(), 4);

  Assert.equal(pm.testPermission(uri, "pear"), 1);
  Assert.equal(pm.testPermission(uri2, "pear"), 2);

  Assert.equal(pm.testPermission(uri, "apple"), 0);
  Assert.equal(pm.testPermission(uri2, "apple"), 0);
  Assert.equal(pm.testPermission(uri3, "apple"), 0);

  Assert.equal(pm.testPermission(uri, "cucumber"), 1);
  Assert.equal(pm.testPermission(uri3, "cucumber"), 3);

  pm.removeByType("cucumber");
  Assert.equal(perm_count(), 2);

  Assert.equal(pm.testPermission(uri, "pear"), 1);
  Assert.equal(pm.testPermission(uri2, "pear"), 2);

  Assert.equal(pm.testPermission(uri, "apple"), 0);
  Assert.equal(pm.testPermission(uri2, "apple"), 0);
  Assert.equal(pm.testPermission(uri3, "apple"), 0);

  Assert.equal(pm.testPermission(uri, "cucumber"), 0);
  Assert.equal(pm.testPermission(uri3, "cucumber"), 0);

  pm.removeByType("pear");
  Assert.equal(perm_count(), 0);

  Assert.equal(pm.testPermission(uri, "pear"), 0);
  Assert.equal(pm.testPermission(uri2, "pear"), 0);

  Assert.equal(pm.testPermission(uri, "apple"), 0);
  Assert.equal(pm.testPermission(uri2, "apple"), 0);
  Assert.equal(pm.testPermission(uri3, "apple"), 0);

  Assert.equal(pm.testPermission(uri, "cucumber"), 0);
  Assert.equal(pm.testPermission(uri3, "cucumber"), 0);

  function perm_count() {
    let enumerator = pm.enumerator;
    let count = 0;
    while (enumerator.hasMoreElements()) {
      count++;
      enumerator.getNext();
    }

    return count;
  }
}
