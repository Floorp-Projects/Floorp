/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  // initialize the permission manager service
  let pm = Cc["@mozilla.org/permissionmanager;1"].
        getService(Ci.nsIPermissionManager);

  Assert.equal(perm_count(), 0);

  // add some permissions
  let uri = NetUtil.newURI("http://amazon.com:8080/foobarbaz");
  let uri2 = NetUtil.newURI("http://google.com:2048/quxx");

  pm.add(uri, "apple", 0);
  pm.add(uri, "apple", 3);
  pm.add(uri, "pear", 3);
  pm.add(uri, "pear", 1);
  pm.add(uri, "cucumber", 1);
  pm.add(uri, "cucumber", 1);
  pm.add(uri, "cucumber", 1);

  pm.add(uri2, "apple", 2);
  pm.add(uri2, "pear", 0);
  pm.add(uri2, "pear", 2);

  // Make sure that removePermission doesn't remove more than one permission each time
  Assert.equal(perm_count(), 5);

  remove_one_by_type("apple");
  Assert.equal(perm_count(), 4);

  remove_one_by_type("apple");
  Assert.equal(perm_count(), 3);

  remove_one_by_type("pear");
  Assert.equal(perm_count(), 2);

  remove_one_by_type("cucumber");
  Assert.equal(perm_count(), 1);

  remove_one_by_type("pear");
  Assert.equal(perm_count(), 0);


  function perm_count() {
    let enumerator = pm.enumerator;
    let count = 0;
    while (enumerator.hasMoreElements()) {
      count++;
      enumerator.getNext();
    }

    return count;
  }

  function remove_one_by_type(type) {
    let enumerator = pm.enumerator;
    while (enumerator.hasMoreElements()) {
      let it = enumerator.getNext().QueryInterface(Ci.nsIPermission);
      if (it.type == type) {
        pm.removePermission(it);
        break;
      }
    }
  }
}
