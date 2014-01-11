function run_test() {
  var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"].getService(
    Components.interfaces.nsIScriptSecurityManager);

  do_check_true(secMan.isSystemPrincipal(secMan.getObjectPrincipal({})));
}
