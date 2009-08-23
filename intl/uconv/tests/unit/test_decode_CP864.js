// Tests conversion from IBM864 to Unicode
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š ¡¢£¤¥§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış";
    
const expectedString = " !\"#$\u066a&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00b0\u00b7\u2219\u221a\u2592\u2500\u2502\u253c\u2524\u252c\u251c\u2534\u2510\u250c\u2514\u2518\u03b2\u221e\u03c6\u00b1\u00bd\u00bc\u2248\u00ab\u00bb\ufef7\ufef8\ufefb\ufefc\u00a0\u00ad\ufe82\u00a3\u00a4\ufe84\u20ac\ufe8e\ufe8f\ufe95\ufe99\u060c\ufe9d\ufea1\ufea5\u0660\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u0669\ufed1\u061b\ufeb1\ufeb5\ufeb9\u061f\u00a2\ufe80\ufe81\ufe83\ufe85\ufeca\ufe8b\ufe8d\ufe91\ufe93\ufe97\ufe9b\ufe9f\ufea3\ufea7\ufea9\ufeab\ufead\ufeaf\ufeb3\ufeb7\ufebb\ufebf\ufec1\ufec5\ufecb\ufecf\u00a6\u00ac\u00f7\u00d7\ufec9\u0640\ufed3\ufed7\ufedb\ufedf\ufee3\ufee7\ufeeb\ufeed\ufeef\ufef3\ufebd\ufecc\ufece\ufecd\ufee1\ufe7d\u0651\ufee5\ufee9\ufeec\ufef0\ufef2\ufed0\ufed5\ufef5\ufef6\ufedd\ufed9\ufef1\u25a0";

function test_conversion(converter, charset) {
    converter.charset = charset;
    return converter.ConvertToUnicode(inString) + converter.Finish();
}

function run_test() {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");
    var converter = new ScriptableUnicodeConverter();

    do_check_eq(test_conversion(converter, "IBM864"), expectedString);
    do_check_eq(test_conversion(converter, "csIBM864"), expectedString);
    do_check_eq(test_conversion(converter, "CP864"), expectedString);
    do_check_eq(test_conversion(converter, "864"), expectedString);
}
