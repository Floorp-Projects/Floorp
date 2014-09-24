/* Tests conversion from Unicode to HZ-GB-2312 (bug 367026)
 *
 * Notes:
 * HZ-GB-2312 is a 7-bit encoding of the GB2312 simplified Chinese character
 * set. It uses the escape sequences "~{" to mark the start of GB encoded text
 * and "~}" to mark the end.
 *
 * See http://www.ietf.org/rfc/rfc1843.txt
 */

load('CharsetConversionTests.js');

const inASCII = "Hello World";
const inHanzi = "\u4E00";
const inMixed = "Hello \u4E00 World";
    
const expectedASCII = "Hello World";
const expectedHanzi = "~{R;~}";
const expectedMixed = "Hello ~{R;~} World";

const charset = "HZ-GB-2312";
    
function run_test() {
    var converter = CreateScriptableConverter();
    converter.isInternal = true;

    checkEncode(converter, charset, inASCII, expectedASCII);
    checkEncode(converter, charset, inMixed, expectedMixed);
    checkEncode(converter, charset, inHanzi, expectedHanzi);
}
