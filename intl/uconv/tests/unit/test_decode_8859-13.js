// Tests conversion from ISO-8859-13 to Unicode
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00a0\u201d\u00a2\u00a3\u00a4\u201e\u00a6\u00a7\u00d8\u00a9\u0156\u00ab\u00ac\u00ad\u00ae\u00c6\u00b0\u00b1\u00b2\u00b3\u201c\u00b5\u00b6\u00b7\u00f8\u00b9\u0157\u00bb\u00bc\u00bd\u00be\u00e6\u0104\u012e\u0100\u0106\u00c4\u00c5\u0118\u0112\u010c\u00c9\u0179\u0116\u0122\u0136\u012a\u013b\u0160\u0143\u0145\u00d3\u014c\u00d5\u00d6\u00d7\u0172\u0141\u015a\u016a\u00dc\u017b\u017d\u00df\u0105\u012f\u0101\u0107\u00e4\u00e5\u0119\u0113\u010d\u00e9\u017a\u0117\u0123\u0137\u012b\u013c\u0161\u0144\u0146\u00f3\u014d\u00f5\u00f6\u00f7\u0173\u0142\u015b\u016b\u00fc\u017c\u017e\u2019";

const charset = "ISO-8859-13";

function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;

    var outString = converter.ConvertToUnicode(inString) + converter.Finish();
    do_check_eq(outString, expectedString);
}
