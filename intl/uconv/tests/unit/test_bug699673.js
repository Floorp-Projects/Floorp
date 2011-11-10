// Tests whether nsTextToSubURI does UTF-16 unescaping (it shouldn't)
 function run_test()
{
  var testURI = "data:text/html,%FE%FF";
  var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"].getService(Components.interfaces.nsITextToSubURI);
  do_check_eq(textToSubURI.unEscapeNonAsciiURI("UTF-16", testURI), testURI);
}
