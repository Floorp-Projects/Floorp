// Tests conversion from x-mac-greek to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c4\u00b9\u00b2\u00c9\u00b3\u00d6\u00dc\u0385\u00e0\u00e2\u00e4\u0384\u00a8\u00e7\u00e9\u00e8\u00ea\u00eb\u00a3\u2122\u00ee\u00ef\u2022\u00bd\u2030\u00f4\u00f6\u00a6\u20ac\u00f9\u00fb\u00fc\u2020\u0393\u0394\u0398\u039b\u039e\u03a0\u00df\u00ae\u00a9\u03a3\u03aa\u00a7\u2260\u00b0\u00b7\u0391\u00b1\u2264\u2265\u00a5\u0392\u0395\u0396\u0397\u0399\u039a\u039c\u03a6\u03ab\u03a8\u03a9\u03ac\u039d\u00ac\u039f\u03a1\u2248\u03a4\u00ab\u00bb\u2026\u00a0\u03a5\u03a7\u0386\u0388\u0153\u2013\u2015\u201c\u201d\u2018\u2019\u00f7\u0389\u038a\u038c\u038e\u03ad\u03ae\u03af\u03cc\u038f\u03cd\u03b1\u03b2\u03c8\u03b4\u03b5\u03c6\u03b3\u03b7\u03b9\u03be\u03ba\u03bb\u03bc\u03bd\u03bf\u03c0\u03ce\u03c1\u03c3\u03c4\u03b8\u03c9\u03c2\u03c7\u03c5\u03b6\u03ca\u03cb\u0390\u03b0\u00ad";

const aliases = [ "x-mac-greek" ];

function run_test() {
  testDecodeAliasesInternal();
}
