/* Tests conversion from Unicode to HZ-GB-2312 (bug 367026)
 *
 * Notes:
 * HZ-GB-2312 is a 7-bit encoding of the GB2312 simplified Chinese character
 * set. It uses the escape sequences "~{" to mark the start of GB encoded text
 * and "~}" to mark the end.
 *
 * See http://www.ietf.org/rfc/rfc1843.txt
 */

const inASCII = "Hello World";
const inHanzi = "\u4E00";
const inMixed = "Hello \u4E00 World";
    
const expectedASCII = "Hello World";
const expectedHanzi = "~{R;~}";
const expectedMixed = "Hello ~{R;~} World";

const charset = "HZ-GB-2312";
    
function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;

    var outASCII = converter.ConvertFromUnicode(inASCII) + converter.Finish();
    do_check_eq(outASCII, expectedASCII);

    var outMixed = converter.ConvertFromUnicode(inMixed) + converter.Finish();
    do_check_eq(outMixed, expectedMixed);

    var outHanzi = converter.ConvertFromUnicode(inHanzi) + converter.Finish();
    do_check_eq(outHanzi, expectedHanzi);
}
