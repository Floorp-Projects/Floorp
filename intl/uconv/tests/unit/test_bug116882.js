/* Tests conversion of undefined and illegal sequences from Shift-JIS
 *  to Unicode (bug 116882)
 */

const inText = "\xfd\xfe\xff\x81\x20\x81\x3f\x86\x3c";
const expectedText = "\uf8f1\uf8f2\uf8f3\ufffd \ufffd?\ufffd<";
const charset = "Shift_JIS";
    
load('CharsetConversionTests.js');

function run_test() {
  checkDecode(CreateScriptableConverter(), charset, inText, expectedText);
}
