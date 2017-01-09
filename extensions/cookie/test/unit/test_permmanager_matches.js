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
  let uri0 = NetUtil.newURI("http://google.com/search?q=foo#hashtag");
  let uri1 = NetUtil.newURI("http://hangouts.google.com/subdir");
  let uri2 = NetUtil.newURI("http://google.org/");
  let uri3 = NetUtil.newURI("https://google.com/some/random/subdirectory");
  let uri4 = NetUtil.newURI("https://hangouts.google.com/#!/hangout");
  let uri5 = NetUtil.newURI("http://google.com:8096/");

  let uri0_n_n = secMan.createCodebasePrincipal(uri0, {});
  let uri1_n_n = secMan.createCodebasePrincipal(uri1, {});
  let uri2_n_n = secMan.createCodebasePrincipal(uri2, {});
  let uri3_n_n = secMan.createCodebasePrincipal(uri3, {});
  let uri4_n_n = secMan.createCodebasePrincipal(uri4, {});
  let uri5_n_n = secMan.createCodebasePrincipal(uri5, {});

  let attrs = {appId: 1000};
  let uri0_1000_n = secMan.createCodebasePrincipal(uri0, attrs);
  let uri1_1000_n = secMan.createCodebasePrincipal(uri1, attrs);
  let uri2_1000_n = secMan.createCodebasePrincipal(uri2, attrs);
  let uri3_1000_n = secMan.createCodebasePrincipal(uri3, attrs);
  let uri4_1000_n = secMan.createCodebasePrincipal(uri4, attrs);
  let uri5_1000_n = secMan.createCodebasePrincipal(uri5, attrs);

  attrs = {appId: 1000, inIsolatedMozBrowser: true};
  let uri0_1000_y = secMan.createCodebasePrincipal(uri0, attrs);
  let uri1_1000_y = secMan.createCodebasePrincipal(uri1, attrs);
  let uri2_1000_y = secMan.createCodebasePrincipal(uri2, attrs);
  let uri3_1000_y = secMan.createCodebasePrincipal(uri3, attrs);
  let uri4_1000_y = secMan.createCodebasePrincipal(uri4, attrs);
  let uri5_1000_y = secMan.createCodebasePrincipal(uri5, attrs);

  attrs = {appId: 2000};
  let uri0_2000_n = secMan.createCodebasePrincipal(uri0, attrs);
  let uri1_2000_n = secMan.createCodebasePrincipal(uri1, attrs);
  let uri2_2000_n = secMan.createCodebasePrincipal(uri2, attrs);
  let uri3_2000_n = secMan.createCodebasePrincipal(uri3, attrs);
  let uri4_2000_n = secMan.createCodebasePrincipal(uri4, attrs);
  let uri5_2000_n = secMan.createCodebasePrincipal(uri5, attrs);

  attrs = {appId: 2000, inIsolatedMozBrowser: true};
  let uri0_2000_y = secMan.createCodebasePrincipal(uri0, attrs);
  let uri1_2000_y = secMan.createCodebasePrincipal(uri1, attrs);
  let uri2_2000_y = secMan.createCodebasePrincipal(uri2, attrs);
  let uri3_2000_y = secMan.createCodebasePrincipal(uri3, attrs);
  let uri4_2000_y = secMan.createCodebasePrincipal(uri4, attrs);
  let uri5_2000_y = secMan.createCodebasePrincipal(uri5, attrs);

  attrs = {userContextId: 1};
  let uri0_1 = secMan.createCodebasePrincipal(uri0, attrs);
  let uri1_1 = secMan.createCodebasePrincipal(uri1, attrs);
  let uri2_1 = secMan.createCodebasePrincipal(uri2, attrs);
  let uri3_1 = secMan.createCodebasePrincipal(uri3, attrs);
  let uri4_1 = secMan.createCodebasePrincipal(uri4, attrs);
  let uri5_1 = secMan.createCodebasePrincipal(uri5, attrs);

  attrs = {firstPartyDomain: "cnn.com"};
  let uri0_cnn = secMan.createCodebasePrincipal(uri0, attrs);
  let uri1_cnn = secMan.createCodebasePrincipal(uri1, attrs);
  let uri2_cnn = secMan.createCodebasePrincipal(uri2, attrs);
  let uri3_cnn = secMan.createCodebasePrincipal(uri3, attrs);
  let uri4_cnn = secMan.createCodebasePrincipal(uri4, attrs);
  let uri5_cnn = secMan.createCodebasePrincipal(uri5, attrs);

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
  pm.addFromPrincipal(uri0_1, "test/matches", pm.ALLOW_ACTION);
  let perm_1 = pm.getPermissionObject(uri0_n_n, "test/matches", true);
  pm.addFromPrincipal(uri0_cnn, "test/matches", pm.ALLOW_ACTION);
  let perm_cnn = pm.getPermissionObject(uri0_n_n, "test/matches", true);

  matches_always(perm_n_n, [uri0_n_n, uri0_1, uri0_cnn]);
  matches_weak(perm_n_n, [uri1_n_n, uri1_1, uri1_cnn]);
  matches_never(perm_n_n, [uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                           uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                           uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                           uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                           uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                           uri2_1, uri3_1, uri4_1, uri5_1,
                           uri2_cnn, uri3_cnn, uri4_cnn, uri5_cnn]);

  matches_always(perm_1000_n, [uri0_1000_n]);
  matches_weak(perm_1000_n, [uri1_1000_n]);
  matches_never(perm_1000_n, [uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                              uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                              uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                              uri0_1, uri1_1, uri2_1, uri3_1, uri4_1, uri5_1,
                              uri0_cnn, uri1_cnn, uri2_cnn, uri3_cnn, uri4_cnn, uri5_cnn]);

  matches_always(perm_1000_y, [uri0_1000_y]);
  matches_weak(perm_1000_y, [uri1_1000_y]);
  matches_never(perm_1000_y, [uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                              uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                              uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                              uri0_1, uri1_1, uri2_1, uri3_1, uri4_1, uri5_1,
                              uri0_cnn, uri1_cnn, uri2_cnn, uri3_cnn, uri4_cnn, uri5_cnn]);

  matches_always(perm_2000_n, [uri0_2000_n]);
  matches_weak(perm_2000_n, [uri1_2000_n]);
  matches_never(perm_2000_n, [uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                              uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                              uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                              uri0_1, uri1_1, uri2_1, uri3_1, uri4_1, uri5_1,
                              uri0_cnn, uri1_cnn, uri2_cnn, uri3_cnn, uri4_cnn, uri5_cnn]);

  matches_always(perm_2000_y, [uri0_2000_y]);
  matches_weak(perm_2000_y, [uri1_2000_y]);
  matches_never(perm_2000_y, [uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                              uri0_n_n, uri1_n_n, uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                              uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                              uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                              uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                              uri0_1, uri1_1, uri2_1, uri3_1, uri4_1, uri5_1,
                              uri0_cnn, uri1_cnn, uri2_cnn, uri3_cnn, uri4_cnn, uri5_cnn]);

  matches_always(perm_1, [uri0_n_n, uri0_1, uri0_cnn]);
  matches_weak(perm_1, [uri1_n_n, uri1_1, uri1_cnn]);
  matches_never(perm_1, [uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                         uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                         uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                         uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                         uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                         uri2_1, uri3_1, uri4_1, uri5_1,
                         uri2_cnn, uri3_cnn, uri4_cnn, uri5_cnn]);

  matches_always(perm_cnn, [uri0_n_n, uri0_1, uri0_cnn]);
  matches_weak(perm_cnn, [uri1_n_n, uri1_1, uri1_cnn]);
  matches_never(perm_cnn, [uri2_n_n, uri3_n_n, uri4_n_n, uri5_n_n,
                           uri0_1000_n, uri1_1000_n, uri2_1000_n, uri3_1000_n, uri4_1000_n, uri5_1000_n,
                           uri0_1000_y, uri1_1000_y, uri2_1000_y, uri3_1000_y, uri4_1000_y, uri5_1000_y,
                           uri0_2000_n, uri1_2000_n, uri2_2000_n, uri3_2000_n, uri4_2000_n, uri5_2000_n,
                           uri0_2000_y, uri1_2000_y, uri2_2000_y, uri3_2000_y, uri4_2000_y, uri5_2000_y,
                           uri2_1, uri3_1, uri4_1, uri5_1,
                           uri2_cnn, uri3_cnn, uri4_cnn, uri5_cnn]);

  // Clean up!
  pm.removeAll();
}
