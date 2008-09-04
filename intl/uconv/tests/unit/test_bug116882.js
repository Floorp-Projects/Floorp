/* Tests conversion of undefined and illegal sequences from Shift-JIS
 *  to Unicode (bug 116882)
 */

const inText = "\xfd\xfe\xff\x81\x20\x81\x3f\x86\x3c";
const expectedText = "\uf8f1\uf8f2\uf8f3\u30fb\u30fb\u30fb";
const charset = "Shift_JIS";
    
function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;

    var outText = converter.ConvertToUnicode(inText) + converter.Finish();
    do_check_eq(outText, expectedText);
}
