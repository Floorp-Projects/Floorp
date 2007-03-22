// Tests conversion from Unicode to ISO-8859-14
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00a0\u1e02\u1e03\u00a3\u010a\u010b\u1e0a\u00a7\u1e80\u00a9\u1e82\u1e0b\u1ef2\u00ad\u00ae\u0178\u1e1e\u1e1f\u0120\u0121\u1e40\u1e41\u00b6\u1e56\u1e81\u1e57\u1e83\u1e60\u1ef3\u1e84\u1e85\u1e61\u00c0\u00c1\u00c2\u00c3\u00c4\u00c5\u00c6\u00c7\u00c8\u00c9\u00ca\u00cb\u00cc\u00cd\u00ce\u00cf\u0174\u00d1\u00d2\u00d3\u00d4\u00d5\u00d6\u1e6a\u00d8\u00d9\u00da\u00db\u00dc\u00dd\u0176\u00df\u00e0\u00e1\u00e2\u00e3\u00e4\u00e5\u00e6\u00e7\u00e8\u00e9\u00ea\u00eb\u00ec\u00ed\u00ee\u00ef\u0175\u00f1\u00f2\u00f3\u00f4\u00f5\u00f6\u1e6b\u00f8\u00f9\u00fa\u00fb\u00fc\u00fd\u0177\u00ff";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";

const charset = "ISO-8859-14";

function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;

    var outString = converter.ConvertFromUnicode(inString) + converter.Finish();
    do_check_eq(outString, expectedString);
}
