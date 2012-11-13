// Tests conversion of 8-byte decomposed Hangul from EUC-KR (and variants)
// to Unicode, including invalid input
        
load('CharsetConversionTests.js');
load('hangulTestStrings.js');

const aliases = [ "euc-kr", "EUC-KR", "iso-ir-149", "ks_c_5601-1989", "ksc_5601", "ksc5601", "korean", "csksc56011987", "5601", "windows-949", "ks_c_5601-1987", "cseuckr"];

function to8byteHangul(byte3, byte5, byte7)
{
    return String.fromCharCode(0xa4, 0xd4, 0xa4, byte3, 0xa4, byte5, 0xa4, byte7);
}

function run_test() {
    var converter = CreateScriptableConverter();
    for (var i = 0; i < aliases.length; ++i) {
        var row = 0;
        for (var byte3 = 0xa0; byte3 < 0xc0; ++byte3) {
            for (var byte5 = 0xbe; byte5 < 0xd5; ++byte5) {
                var inString = " row " + byte3.toString(16) + "_" +
                    byte5.toString(16) + "_: ";
                for (var byte7 = 0xa0; byte7 < 0xc0; ++byte7) {
                    inString += to8byteHangul(byte3, byte5, byte7) + " ";
                }
                inString += to8byteHangul(byte3, byte5, 0xd4) + " ";
                checkDecode(converter, aliases[i], inString, expectedStrings[row++]);
            }
        }
        do_check_eq(row, expectedStrings.length);
    }
}
