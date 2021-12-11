/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that permissions work for file:// URIs (aka local files).

function getPrincipalFromURIString(uriStr) {
  let uri = NetUtil.newURI(uriStr);
  return Services.scriptSecurityManager.createContentPrincipal(uri, {});
}

function run_test() {
  let pm = Services.perms;

  // If we add a permission to a file:// URI, the test should return true.
  let principal = getPrincipalFromURIString("file:///foo/bar");
  pm.addFromPrincipal(principal, "test/local-files", pm.ALLOW_ACTION, 0, 0);
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "test/local-files"),
    pm.ALLOW_ACTION
  );

  // Another file:// URI should have the same permission.
  let witnessPrincipal = getPrincipalFromURIString("file:///bar/foo");
  Assert.equal(
    pm.testPermissionFromPrincipal(witnessPrincipal, "test/local-files"),
    pm.UNKNOWN_ACTION
  );

  // Giving "file:///" a permission shouldn't give it to all file:// URIs.
  let rootPrincipal = getPrincipalFromURIString("file:///");
  pm.addFromPrincipal(rootPrincipal, "test/local-files", pm.ALLOW_ACTION, 0, 0);
  Assert.equal(
    pm.testPermissionFromPrincipal(witnessPrincipal, "test/local-files"),
    pm.UNKNOWN_ACTION
  );

  // Giving "file://" a permission shouldn't give it to all file:// URIs.
  let schemeRootPrincipal = getPrincipalFromURIString("file://");
  pm.addFromPrincipal(
    schemeRootPrincipal,
    "test/local-files",
    pm.ALLOW_ACTION,
    0,
    0
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(witnessPrincipal, "test/local-files"),
    pm.UNKNOWN_ACTION
  );

  // Giving 'node' a permission shouldn't give it to its 'children'.
  let fileInDirPrincipal = getPrincipalFromURIString(
    "file:///foo/bar/foobar.txt"
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(fileInDirPrincipal, "test/local-files"),
    pm.UNKNOWN_ACTION
  );

  // Revert "file:///foo/bar" permission and check that it has been correctly taken into account.
  pm.removeFromPrincipal(principal, "test/local-files");
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "test/local-files"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(witnessPrincipal, "test/local-files"),
    pm.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(fileInDirPrincipal, "test/local-files"),
    pm.UNKNOWN_ACTION
  );
}
