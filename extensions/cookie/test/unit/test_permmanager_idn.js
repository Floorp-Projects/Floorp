/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getPrincipalFromDomain(aDomain) {
  return Cc["@mozilla.org/scriptsecuritymanager;1"]
           .getService(Ci.nsIScriptSecurityManager)
           .getNoAppCodebasePrincipal(NetUtil.newURI("http://" + aDomain));
}

function run_test() {
  let profile = do_get_profile();
  let pm = Services.permissions;
  let perm = 'test-idn';

  // We create three principal linked to IDN.
  // One with just a domain, one with a subdomain and one with the TLD
  // containing a UTF-8 character.
  let mainDomainPrincipal = getPrincipalFromDomain("fôû.com");
  let subDomainPrincipal = getPrincipalFromDomain("fôô.bàr.com");
  let tldPrincipal = getPrincipalFromDomain("fôû.bàr.côm");

  // We add those to the permission manager.
  pm.addFromPrincipal(mainDomainPrincipal, perm, pm.ALLOW_ACTION, 0, 0);
  pm.addFromPrincipal(subDomainPrincipal, perm, pm.ALLOW_ACTION, 0, 0);
  pm.addFromPrincipal(tldPrincipal, perm, pm.ALLOW_ACTION, 0, 0);

  // They should obviously be there now..
  do_check_eq(pm.testPermissionFromPrincipal(mainDomainPrincipal, perm), pm.ALLOW_ACTION);
  do_check_eq(pm.testPermissionFromPrincipal(subDomainPrincipal, perm), pm.ALLOW_ACTION);
  do_check_eq(pm.testPermissionFromPrincipal(tldPrincipal, perm), pm.ALLOW_ACTION);

  // We do the same thing with the puny-encoded versions of the IDN.
  let punyMainDomainPrincipal = getPrincipalFromDomain('xn--f-xgav.com');
  let punySubDomainPrincipal = getPrincipalFromDomain('xn--f-xgaa.xn--br-jia.com');
  let punyTldPrincipal = getPrincipalFromDomain('xn--f-xgav.xn--br-jia.xn--cm-8ja');

  // Those principals should have the permission granted too.
  do_check_eq(pm.testPermissionFromPrincipal(punyMainDomainPrincipal, perm), pm.ALLOW_ACTION);
  do_check_eq(pm.testPermissionFromPrincipal(punySubDomainPrincipal, perm), pm.ALLOW_ACTION);
  do_check_eq(pm.testPermissionFromPrincipal(punyTldPrincipal, perm), pm.ALLOW_ACTION);

  // However, those two principals shouldn't be allowed because they are like
  // the IDN but without the UT8-8 characters.
  let witnessPrincipal = getPrincipalFromDomain("foo.com");
  do_check_eq(pm.testPermissionFromPrincipal(witnessPrincipal, perm), pm.UNKNOWN_ACTION);
  witnessPrincipal = getPrincipalFromDomain("foo.bar.com");
  do_check_eq(pm.testPermissionFromPrincipal(witnessPrincipal, perm), pm.UNKNOWN_ACTION);
}