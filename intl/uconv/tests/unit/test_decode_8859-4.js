// Tests conversion from ISO-8859-4 to Unicode
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00a0\u0104\u0138\u0156\u00a4\u0128\u013b\u00a7\u00a8\u0160\u0112\u0122\u0166\u00ad\u017d\u00af\u00b0\u0105\u02db\u0157\u00b4\u0129\u013c\u02c7\u00b8\u0161\u0113\u0123\u0167\u014a\u017e\u014b\u0100\u00c1\u00c2\u00c3\u00c4\u00c5\u00c6\u012e\u010c\u00c9\u0118\u00cb\u0116\u00cd\u00ce\u012a\u0110\u0145\u014c\u0136\u00d4\u00d5\u00d6\u00d7\u00d8\u0172\u00da\u00db\u00dc\u0168\u016a\u00df\u0101\u00e1\u00e2\u00e3\u00e4\u00e5\u00e6\u012f\u010d\u00e9\u0119\u00eb\u0117\u00ed\u00ee\u012b\u0111\u0146\u014d\u0137\u00f4\u00f5\u00f6\u00f7\u00f8\u0173\u00fa\u00fb\u00fc\u0169\u016b\u02d9";

const charset = "ISO-8859-4";

function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;

    var outString = converter.ConvertToUnicode(inString) + converter.Finish();
    do_check_eq(outString, expectedString);
}
