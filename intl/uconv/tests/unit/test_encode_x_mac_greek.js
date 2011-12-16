// Tests conversion from Unicode to x-mac-greek
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c4\u00b9\u00b2\u00c9\u00b3\u00d6\u00dc\u0385\u00e0\u00e2\u00e4\u0384\u00a8\u00e7\u00e9\u00e8\u00ea\u00eb\u00a3\u2122\u00ee\u00ef\u2022\u00bd\u2030\u00f4\u00f6\u00a6\u20ac\u00f9\u00fb\u00fc\u2020\u0393\u0394\u0398\u039b\u039e\u03a0\u00df\u00ae\u00a9\u03a3\u03aa\u00a7\u2260\u00b0\u00b7\u0391\u00b1\u2264\u2265\u00a5\u0392\u0395\u0396\u0397\u0399\u039a\u039c\u03a6\u03ab\u03a8\u03a9\u03ac\u039d\u00ac\u039f\u03a1\u2248\u03a4\u00ab\u00bb\u2026\u00a0\u03a5\u03a7\u0386\u0388\u0153\u2013\u2015\u201c\u201d\u2018\u2019\u00f7\u0389\u038a\u038c\u038e\u03ad\u03ae\u03af\u03cc\u038f\u03cd\u03b1\u03b2\u03c8\u03b4\u03b5\u03c6\u03b3\u03b7\u03b9\u03be\u03ba\u03bb\u03bc\u03bd\u03bf\u03c0\u03ce\u03c1\u03c3\u03c4\u03b8\u03c9\u03c2\u03c7\u03c5\u03b6\u03ca\u03cb\u0390\u03b0\u00ad";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";

const aliases = [ "x-mac-greek" ];

function run_test() {
  testEncodeAliases();
}
