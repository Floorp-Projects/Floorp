function run_test() {
  var success = false;
  try {
    ios.newURI("feed:javascript:alert('hi');", null, null);
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a feed:javascript: URI");

  success = false;
  try {
    ios.newURI("feed:data:text/html,hi", null, null);
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a feed:data: URI");

  success = false;
  try {
    ios.newURI("pcast:javascript:alert('hi');", null, null);
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a pcast:javascript: URI");

  success = false;
  try {
    ios.newURI("pcast:data:text/html,hi", null, null);
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a pcast:data: URI");

}
