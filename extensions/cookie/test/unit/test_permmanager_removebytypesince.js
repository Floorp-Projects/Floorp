/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  // initialize the permission manager service
  let pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);

  Assert.equal(perm_count(), 0);

  // add some permissions
  let uri = Services.io.newURI("http://amazon.com:8080/foobarbaz");
  let uri2 = Services.io.newURI("http://google.com:2048/quxx");
  let uri3 = Services.io.newURI("https://google.com/search");

  pm.add(uri, "apple", 3);
  pm.add(uri, "pear", 1);
  pm.add(uri, "cucumber", 1);

  // sleep briefly, then record the time - we'll remove some permissions since then.
  await new Promise(resolve => do_timeout(20, resolve));

  let since = Date.now();

  // *sob* - on Windows at least, the now recorded by nsPermissionManager.cpp
  // might be a couple of ms *earlier* than what JS sees.  So another sleep
  // to ensure our |since| is greater than the time of the permissions we
  // are now adding.  Sadly this means we'll never be able to test when since
  // exactly equals the modTime, but there you go...
  await new Promise(resolve => do_timeout(20, resolve));

  pm.add(uri2, "apple", 2);
  pm.add(uri2, "pear", 2);

  pm.add(uri3, "cucumber", 3);
  pm.add(uri3, "apple", 1);

  Assert.equal(perm_count(), 7);

  pm.removeByTypeSince("apple", since);

  Assert.equal(perm_count(), 5);

  Assert.equal(pm.testPermission(uri, "pear"), 1);
  Assert.equal(pm.testPermission(uri2, "pear"), 2);

  Assert.equal(pm.testPermission(uri, "apple"), 3);
  Assert.equal(pm.testPermission(uri2, "apple"), 0);
  Assert.equal(pm.testPermission(uri3, "apple"), 0);
 
  Assert.equal(pm.testPermission(uri, "cucumber"), 1);
  Assert.equal(pm.testPermission(uri3, "cucumber"), 3);

  pm.removeByTypeSince("cucumber", since);
  Assert.equal(perm_count(), 4);

  Assert.equal(pm.testPermission(uri, "pear"), 1);
  Assert.equal(pm.testPermission(uri2, "pear"), 2);
  
  Assert.equal(pm.testPermission(uri, "apple"), 3);
  Assert.equal(pm.testPermission(uri2, "apple"), 0);
  Assert.equal(pm.testPermission(uri3, "apple"), 0);
  
  Assert.equal(pm.testPermission(uri, "cucumber"), 1);
  Assert.equal(pm.testPermission(uri3, "cucumber"), 0);
  
  pm.removeByTypeSince("pear", since);
  Assert.equal(perm_count(), 3);
  
  Assert.equal(pm.testPermission(uri, "pear"), 1);
  Assert.equal(pm.testPermission(uri2, "pear"), 0);
  
  Assert.equal(pm.testPermission(uri, "apple"), 3);
  Assert.equal(pm.testPermission(uri2, "apple"), 0);
  Assert.equal(pm.testPermission(uri3, "apple"), 0);
  
  Assert.equal(pm.testPermission(uri, "cucumber"), 1);
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
});
