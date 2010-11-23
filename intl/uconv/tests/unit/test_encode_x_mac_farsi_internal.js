// Tests conversion from Unicode to x-mac-farsi

load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c4\u00a0\u00c7\u00c9\u00d1\u00d6\u00dc\u00e1\u00e0\u00e2\u00e4\u06ba\u00ab\u00e7\u00e9\u00e8\u00ea\u00eb\u00ed\u2026\u00ee\u00ef\u00f1\u00f3\u00bb\u00f4\u00f6\u00f7\u00fa\u00f9\u00fb\u00fc !\"#$\u066a&'()*+\u060c-./\u06f0\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9:\u061b<=>\u061f\u274a\u0621\u0622\u0623\u0624\u0625\u0626\u0627\u0628\u0629\u062a\u062b\u062c\u062d\u062e\u062f\u0630\u0631\u0632\u0633\u0634\u0635\u0636\u0637\u0638\u0639\u063a[\\]^_\u0640\u0641\u0642\u0643\u0644\u0645\u0646\u0647\u0648\u0649\u064a\u064b\u064c\u064d\u064e\u064f\u0650\u0651\u0652\u067e\u0679\u0686\u06d5\u06a4\u06af\u0688\u0691{|}\u0698\u06d2";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ !\"#$¥&'()*+¬-./°±²³´µ¶·¸¹:»<=>¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚ[\\]^_àáâãäåæçèéêëìíîïğñòóôõö÷øùú{|}şÿ";

const aliases = [ "x-mac-farsi" ];

function run_test() {
  testEncodeAliasesInternal();
}
