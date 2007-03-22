// Tests conversion from ISO-8859-3 to Unicode
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¦§¨©ª«¬­¯°±²³´µ¶·¸¹º»¼½¿ÀÁÂÄÅÆÇÈÉÊËÌÍÎÏÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâäåæçèéêëìíîïñòóôõö÷øùúûüışÿ";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00a0\u0126\u02d8\u00a3\u00a4\u0124\u00a7\u00a8\u0130\u015e\u011e\u0134\u00ad\u017b\u00b0\u0127\u00b2\u00b3\u00b4\u00b5\u0125\u00b7\u00b8\u0131\u015f\u011f\u0135\u00bd\u017c\u00c0\u00c1\u00c2\u00c4\u010a\u0108\u00c7\u00c8\u00c9\u00ca\u00cb\u00cc\u00cd\u00ce\u00cf\u00d1\u00d2\u00d3\u00d4\u0120\u00d6\u00d7\u011c\u00d9\u00da\u00db\u00dc\u016c\u015c\u00df\u00e0\u00e1\u00e2\u00e4\u010b\u0109\u00e7\u00e8\u00e9\u00ea\u00eb\u00ec\u00ed\u00ee\u00ef\u00f1\u00f2\u00f3\u00f4\u0121\u00f6\u00f7\u011d\u00f9\u00fa\u00fb\u00fc\u016d\u015d\u02d9";

const charset = "ISO-8859-3";

function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;

    var outString = converter.ConvertToUnicode(inString) + converter.Finish();
    do_check_eq(outString, expectedString);
}
