var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;

function test_formatdatetime_return()
{
  var dateConv = Cc["@mozilla.org/intl/scriptabledateformat;1"].
    getService(Ci.nsIScriptableDateFormat);

  /* Testing if we throw instead of crashing when we are passed 0s. */
  var x = false;
  try {
    dateConv.FormatDate("", Ci.nsIScriptableDateFormat.dateFormatLong,
                        0, 0, 0);
  }
  catch (e if (e.result == Cr.NS_ERROR_INVALID_ARG)) {
    x = true;
  }
  if (!x)
    do_throw("FormatDate didn't throw when passed 0 for its arguments.");
}

function run_test()
{
  test_formatdatetime_return();
}
