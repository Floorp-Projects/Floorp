function run_test() {
  var success = false;
  try {
    Services.io.newURI("feed:javascript:alert('hi');");
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a feed:javascript: URI");

  success = false;
  try {
    Services.io.newURI("feed:data:text/html,hi");
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a feed:data: URI");

  success = false;
  try {
    Services.io.newURI("pcast:javascript:alert('hi');");
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a pcast:javascript: URI");

  success = false;
  try {
    Services.io.newURI("pcast:data:text/html,hi");
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI creating a pcast:data: URI");

}
