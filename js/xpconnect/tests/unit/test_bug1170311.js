const Cu = Components.utils;
function run_test() {
  do_check_throws_nsIException(() => Cu.getObjectPrincipal({}).equals(null), "NS_ERROR_ILLEGAL_VALUE");
  do_check_throws_nsIException(() => Cu.getObjectPrincipal({}).subsumes(null), "NS_ERROR_ILLEGAL_VALUE");
}
