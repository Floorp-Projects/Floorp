// Tests conversion of a single byte from UTF-16 to Unicode
	
const inString = "A";
    
const expectedString = "\ufffd";

const charset = "UTF-16BE";

function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;
    try {
      var outString = converter.ConvertToUnicode(inString) + converter.Finish();
    } catch(e) {
      outString = "\ufffd";
    }
    do_check_eq(outString, expectedString);
}
