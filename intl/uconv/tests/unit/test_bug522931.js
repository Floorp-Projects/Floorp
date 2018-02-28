// crash test with invaild parameter (bug 522931)
function run_test()
{
  var textToSubURI = Cc["@mozilla.org/intl/texttosuburi;1"].getService(Ci.nsITextToSubURI);
  Assert.equal(textToSubURI.UnEscapeAndConvert("UTF-8", null), "");
}
