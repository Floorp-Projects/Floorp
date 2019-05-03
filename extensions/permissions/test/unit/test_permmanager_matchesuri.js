/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function matches_always(perm, uris) {
  uris.forEach((uri) => {
    Assert.ok(perm.matchesURI(uri, true), "perm: " + perm.principal.origin + ", URI: " + uri.spec);
    Assert.ok(perm.matchesURI(uri, false), "perm: " + perm.principal.origin + ", URI: " + uri.spec);
  });
}

function matches_weak(perm, uris) {
  uris.forEach((uri) => {
    Assert.ok(!perm.matchesURI(uri, true), "perm: " + perm.principal.origin + ", URI: " + uri.spec);
    Assert.ok(perm.matchesURI(uri, false), "perm: " + perm.principal.origin + ", URI: " + uri.spec);
  });
}

function matches_never(perm, uris) {
  uris.forEach((uri) => {
    Assert.ok(!perm.matchesURI(uri, true), "perm: " + perm.principal.origin + ", URI: " + uri.spec);
    Assert.ok(!perm.matchesURI(uri, false), "perm: " + perm.principal.origin + ", URI: " + uri.spec);
  });
}

function mk_permission(uri) {
  let pm = Cc["@mozilla.org/permissionmanager;1"].
        getService(Ci.nsIPermissionManager);

  let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
        .getService(Ci.nsIScriptSecurityManager);

  // Get the permission from the principal!
  let principal = secMan.createCodebasePrincipal(uri, {});

  pm.addFromPrincipal(principal, "test/matchesuri", pm.ALLOW_ACTION);
  let permission = pm.getPermissionObject(principal, "test/matchesuri", true);

  return permission;
}

function run_test() {
  // initialize the permission manager service
  let pm = Cc["@mozilla.org/permissionmanager;1"].
        getService(Ci.nsIPermissionManager);

  let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
        .getService(Ci.nsIScriptSecurityManager);

  let fileprefix = "file:///";
  if (Services.appinfo.OS == "WINNT") {
    // Windows rejects files if they don't have a drive. See Bug 1180870
    fileprefix += "c:/";
  }

  // Add some permissions
  let uri0 = NetUtil.newURI("http://google.com:9091/just/a/path");
  let uri1 = NetUtil.newURI("http://hangouts.google.com:9091/some/path");
  let uri2 = NetUtil.newURI("http://google.com:9091/");
  let uri3 = NetUtil.newURI("http://google.org:9091/");
  let uri4 = NetUtil.newURI("http://deeper.hangouts.google.com:9091/");
  let uri5 = NetUtil.newURI("https://google.com/just/a/path");
  let uri6 = NetUtil.newURI("https://hangouts.google.com");
  let uri7 = NetUtil.newURI("https://google.com/");

  let fileuri1 = NetUtil.newURI(fileprefix + "a/file/path");
  let fileuri2 = NetUtil.newURI(fileprefix + "a/file/path/deeper");
  let fileuri3 = NetUtil.newURI(fileprefix + "a/file/otherpath");

  {
    let perm = mk_permission(uri0);
    matches_always(perm, [uri0, uri2]);
    matches_weak(perm, [uri1, uri4]);
    matches_never(perm, [uri3, uri5, uri6, uri7, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(uri1);
    matches_always(perm, [uri1]);
    matches_weak(perm, [uri4]);
    matches_never(perm, [uri0, uri2, uri3, uri5, uri6, uri7, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(uri2);
    matches_always(perm, [uri0, uri2]);
    matches_weak(perm, [uri1, uri4]);
    matches_never(perm, [uri3, uri5, uri6, uri7, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(uri3);
    matches_always(perm, [uri3]);
    matches_weak(perm, []);
    matches_never(perm, [uri1, uri2, uri4, uri5, uri6, uri7, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(uri4);
    matches_always(perm, [uri4]);
    matches_weak(perm, []);
    matches_never(perm, [uri1, uri2, uri3, uri5, uri6, uri7, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(uri5);
    matches_always(perm, [uri5, uri7]);
    matches_weak(perm, [uri6]);
    matches_never(perm, [uri0, uri1, uri2, uri3, uri4, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(uri6);
    matches_always(perm, [uri6]);
    matches_weak(perm, []);
    matches_never(perm, [uri0, uri1, uri2, uri3, uri4, uri5, uri7, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(uri7);
    matches_always(perm, [uri5, uri7]);
    matches_weak(perm, [uri6]);
    matches_never(perm, [uri0, uri1, uri2, uri3, uri4, fileuri1, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(fileuri1);
    matches_always(perm, [fileuri1]);
    matches_weak(perm, []);
    matches_never(perm, [uri0, uri1, uri2, uri3, uri4, uri5, uri6, uri7, fileuri2, fileuri3]);
  }

  {
    let perm = mk_permission(fileuri2);
    matches_always(perm, [fileuri2]);
    matches_weak(perm, []);
    matches_never(perm, [uri0, uri1, uri2, uri3, uri4, uri5, uri6, uri7, fileuri1, fileuri3]);
  }

  {
    let perm = mk_permission(fileuri3);
    matches_always(perm, [fileuri3]);
    matches_weak(perm, []);
    matches_never(perm, [uri0, uri1, uri2, uri3, uri4, uri5, uri6, uri7, fileuri1, fileuri2]);
  }

  // Clean up!
  pm.removeAll();
}
