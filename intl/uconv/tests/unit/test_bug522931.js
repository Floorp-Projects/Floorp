// crash test with invaild parameter (bug 522931)
function run_test()
{
  var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"].getService(Components.interfaces.nsITextToSubURI);
  do_check_eq(textToSubURI.UnEscapeAndConvert("UTF-8", null), "");
}
