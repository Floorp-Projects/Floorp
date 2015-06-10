/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function matches_always(perm, principals) {
  principals.forEach((principal) => {
    do_check_true(perm.matches(principal, true), "perm: " + perm.principal.origin + ", princ: " + principal.origin);
    do_check_true(perm.matches(principal, false), "perm: " + perm.principal.origin + ", princ: " + principal.origin);
  });
}

function matches_weak(perm, principals) {
  principals.forEach((principal) => {
    do_check_false(perm.matches(principal, true), "perm: " + perm.principal.origin + ", princ: " + principal.origin);
    do_check_true(perm.matches(principal, false), "perm: " + perm.principal.origin + ", princ: " + principal.origin);
  });
}

function matches_never(perm, principals) {
  principals.forEach((principal) => {
    do_check_false(perm.matches(principal, true), "perm: " + perm.principal.origin + ", princ: " + principal.origin);
    do_check_false(perm.matches(principal, false), "perm: " + perm.principal.origin + ", princ: " + principal.origin);
  });
}

function run_test() {
  // initialize the permission manager service
  let pm = Cc["@mozilla.org/permissionmanager;1"].
        getService(Ci.nsIPermissionManager);

  let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
        .getService(Ci.nsIScriptSecurityManager);

  // Add some permissions
  let uri0 = NetUtil.newURI("http://google.com/search?q=foo#hashtag", null, null);
  let uri1 = NetUtil.newURI("http://hangouts.google.com/subdir", null, null);
  let uri2 = NetUtil.newURI("http://google.org/", null, null);

  // NOTE: These entries have different ports/schemes, but will be considered
  // the same right now (as permissions are based on host only). With bug 1165263
  // this will be fixed.
  let uri3 = NetUtil.newURI("https://google.com/some/random/subdirectory", null, null);
  let uri4 = NetUtil.newURI("https://hangouts.google.com/#!/hangout", null, null);
  let uri5 = NetUtil.newURI("http://google.com:8096/", null, null);

  let uri0_n_n = secMan.getNoAppCodebasePrincipal(uri0);
  let uri1_n_n = secMan.getNoAppCodebasePrincipal(uri1);
  let uri2_n_n = secMan.getNoAppCodebasePrincipal(uri2);
  let uri3_n_n = secMan.getNoAppCodebasePrincipal(uri3);
  let uri4_n_n = secMan.getNoAppCodebasePrincipal(uri4);
  let uri5_n_n = secMan.getNoAppCodebasePrincipal(uri5);

  let uri0_1000_n = secMan.getAppCodebasePrincipal(uri0, 1000, false);
  let uri1_1000_n = secMan.getAppCodebasePrincipal(uri1, 1000, false);
  let uri2_1000_n = secMan.getAppCodebasePrincipal(uri2, 1000, false);
  let uri3_1000_n = secMan.getAppCodebasePrincipal(uri3, 1000, false);
  let uri4_1000_n = secMan.getAppCodebasePrincipal(uri4, 1000, false);
  let uri5_1000_n = secMan.getAppCodebasePrincipal(uri5, 1000, false);

  let uri0_1000_y = secMan.getAppCodebasePrincipal(uri0, 1000, true);
  let uri1_1000_y = secMan.getAppCodebasePrincipal(uri1, 1000, true);
  let uri2_1000_y = secMan.getAppCodebasePrincipal(uri2, 1000, true);
  let uri3_1000_y = secMan.getAppCodebasePrincipal(uri3, 1000, true);
  let uri4_1000_y = secMan.getAppCodebasePrincipal(uri4, 1000, true);
  let uri5_1000_y = secMan.getAppCodebasePrincipal(uri5, 1000, true);

  let uri0_2000_n = secMan.getAppCodebasePrincipal(uri0, 2000, false);
  let uri1_2000_n = secMan.getAppCodebasePrincipal(uri1, 2000, false);
  let uri2_2000_n = secMan.getAppCodebasePrincipal(uri2, 2000, false);
  let uri3_2000_n = secMan.getAppCodebasePrincipal(uri3, 2000, false);
  let uri4_2000_n = secMan.getAppCodebasePrincipal(uri4, 2000, false);
  let uri5_2000_n = secMan.getAppCodebasePrincipal(uri5, 2000, false);

  let uri0_2000_y = secMan.getAppCodebasePrincipal(uri0, 2000, true);
  let uri1_2000_y = secMan.getAppCodebasePrincipal(uri1, 2000, true);
  let uri2_2000_y = secMan.getAppCodebasePrincipal(uri2, 2000, true);
  let uri3_2000_y = secMan.getAppCodebasePrincipal(uri3, 2000, true);
  let uri4_2000_y = secMan.getAppCodebasePrincipal(uri4, 2000, true);
  let uri5_2000_y = secMan.getAppCodebasePrincipal(uri5, 2000, true);

  pm.addFromPrincipal(uri0_n_n, "test/matches", pm.ALLOW_ACTION);
  let perm_n_n = pm.getPermissionObject(uri0_n_n, "test/matches", true);
  pm.addFromPrincipal(uri0_1000_n, "test/matches", pm.ALLOW_ACTION);
  let perm_1000_n = pm.getPermissionObject(uri0_1000_n, "test/matches", true);
  pm.addFromPrincipal(uri0_1000_y, "test/matches", pm.ALLOW_ACTION);
  let perm_1000_y = pm.getPermissionObject(uri0_1000_y, "test/matches", true);
  pm.addFromPrincipal(uri0_2000_n, "test/matches", pm.ALLOW_ACTION);
  let perm_2000_n = pm.getPermissionObject(uri0_2000_n, "test/matches", true);
  pm.addFromPrincipal(uri0_2000_y, "test/matches", pm.ALLOW_ACTION);
  let perm_2000_y = pm.getPermissionObject(uri0_2000_y, "test/matches", true);

  matches_always(perm_n_n, [uri0_n_n, uri3_n_n, uri5_n_n]);
  matches_weak(perm_n_n, [uri1_n_n, uri4_n_n]);
  matches_never(perm_n_n, [uri2_n_n,
                           uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                           uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                           uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                           uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y]);

  matches_always(perm_1000_n, [uri0_1000_n, uri3_1000_n, uri5_1000_n]);
  matches_weak(perm_1000_n, [uri1_1000_n, uri4_1000_n]);
  matches_never(perm_1000_n, [uri2_1000_n,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                              uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                              uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y]);

  matches_always(perm_1000_y, [uri0_1000_y, uri3_1000_y, uri5_1000_y]);
  matches_weak(perm_1000_y, [uri1_1000_y, uri4_1000_y]);
  matches_never(perm_1000_y, [uri2_1000_y,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                              uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                              uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y]);

  matches_always(perm_2000_n, [uri0_2000_n, uri3_2000_n, uri5_2000_n]);
  matches_weak(perm_2000_n, [uri1_2000_n, uri4_2000_n]);
  matches_never(perm_2000_n, [uri2_2000_n,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                              uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                              uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y]);

  matches_always(perm_2000_y, [uri0_2000_y, uri3_2000_y, uri5_2000_y]);
  matches_weak(perm_2000_y, [uri1_2000_y, uri4_2000_y]);
  matches_never(perm_2000_y, [uri2_2000_y,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                              uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                              uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y]);

  // Clean up!
  pm.removeAll();
}
