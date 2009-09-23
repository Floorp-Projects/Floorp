// Tests conversion from Unicode to x-mac-croatian
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c4\u00c5\u00c7\u00c9\u00d1\u00d6\u00dc\u00e1\u00e0\u00e2\u00e4\u00e3\u00e5\u00e7\u00e9\u00e8\u00ea\u00eb\u00ed\u00ec\u00ee\u00ef\u00f1\u00f3\u00f2\u00f4\u00f6\u00f5\u00fa\u00f9\u00fb\u00fc\u2020\u00b0\u00a2\u00a3\u00a7\u2022\u00b6\u00df\u00ae\u0160\u2122\u00b4\u00a8\u2260\u017d\u00d8\u221e\u00b1\u2264\u2265\u2206\u00b5\u2202\u2211\u220f\u0161\u222b\u00aa\u00ba\u03a9\u017e\u00f8\u00bf\u00a1\u00ac\u221a\u0192\u2248\u0106\u00ab\u010c\u2026\u00a0\u00c0\u00c3\u00d5\u0152\u0153\u0110\u2014\u201c\u201d\u2018\u2019\u00f7\u25ca\uf8ff\u00a9\u2044\u20ac\u2039\u203a\u00c6\u00bb\u2013\u00b7\u201a\u201e\u2030\u00c2\u0107\u00c1\u010d\u00c8\u00cd\u00ce\u00cf\u00cc\u00d3\u00d4\u0111\u00d2\u00da\u00db\u00d9\u0131\u02c6\u02dc\u00af\u03c0\u00cb\u02da\u00b8\u00ca\u00e6\u02c7";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";

const aliases = [ "x-mac-croatian" ];

function run_test() {
  testEncodeAliases();
}
