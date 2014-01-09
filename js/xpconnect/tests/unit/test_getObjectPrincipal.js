function run_test() {
  var secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);
  do_check_true(secMan.isSystemPrincipal(secMan.getObjectPrincipal({})));
}
