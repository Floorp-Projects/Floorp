// Tests conversion from x-mac-ce to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c4\u0100\u0101\u00c9\u0104\u00d6\u00dc\u00e1\u0105\u010c\u00e4\u010d\u0106\u0107\u00e9\u0179\u017a\u010e\u00ed\u010f\u0112\u0113\u0116\u00f3\u0117\u00f4\u00f6\u00f5\u00fa\u011a\u011b\u00fc\u2020\u00b0\u0118\u00a3\u00a7\u2022\u00b6\u00df\u00ae\u00a9\u2122\u0119\u00a8\u2260\u0123\u012e\u012f\u012a\u2264\u2265\u012b\u0136\u2202\u2211\u0142\u013b\u013c\u013d\u013e\u0139\u013a\u0145\u0146\u0143\u00ac\u221a\u0144\u0147\u2206\u00ab\u00bb\u2026\u00a0\u0148\u0150\u00d5\u0151\u014c\u2013\u2014\u201c\u201d\u2018\u2019\u00f7\u25ca\u014d\u0154\u0155\u0158\u2039\u203a\u0159\u0156\u0157\u0160\u201a\u201e\u0161\u015a\u015b\u00c1\u0164\u0165\u00cd\u017d\u017e\u016a\u00d3\u00d4\u016b\u016e\u00da\u016f\u0170\u0171\u0172\u0173\u00dd\u00fd\u0137\u017b\u0141\u017c\u0122\u02c7";

const aliases = [ "x-mac-ce" ];

function run_test() {
  testDecodeAliases();
}
