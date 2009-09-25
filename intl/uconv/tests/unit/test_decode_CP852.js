// Tests conversion from IBM852 to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c7\u00fc\u00e9\u00e2\u00e4\u016f\u0107\u00e7\u0142\u00eb\u0150\u0151\u00ee\u0179\u00c4\u0106\u00c9\u0139\u013a\u00f4\u00f6\u013d\u013e\u015a\u015b\u00d6\u00dc\u0164\u0165\u0141\u00d7\u010d\u00e1\u00ed\u00f3\u00fa\u0104\u0105\u017d\u017e\u0118\u0119\u20ac\u017a\u010c\u015f\u00ab\u00bb\u2591\u2592\u2593\u2502\u2524\u00c1\u00c2\u011a\u015e\u2563\u2551\u2557\u255d\u017b\u017c\u2510\u2514\u2534\u252c\u251c\u2500\u253c\u0102\u0103\u255a\u2554\u2569\u2566\u2560\u2550\u256c\u00a4\u0111\u0110\u010e\u00cb\u010f\u0147\u00cd\u00ce\u011b\u2518\u250c\u2588\u2584\u0162\u016e\u2580\u00d3\u00df\u00d4\u0143\u0144\u0148\u0160\u0161\u0154\u00da\u0155\u0170\u00fd\u00dd\u0163\u00b4\u00ad\u02dd\u02db\u02c7\u02d8\u00a7\u00f7\u00b8\u00b0\u00a8\u02d9\u0171\u0158\u0159\u25a0\u00a0";

const aliases = [ "IBM852", "ibm852", "cp852", "852", "csibm852" ];

function run_test() {
  testDecodeAliases();
}
