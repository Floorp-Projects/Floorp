// Tests conversion from Unicode to x-mac-ce
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c4\u0100\u0101\u00c9\u0104\u00d6\u00dc\u00e1\u0105\u010c\u00e4\u010d\u0106\u0107\u00e9\u0179\u017a\u010e\u00ed\u010f\u0112\u0113\u0116\u00f3\u0117\u00f4\u00f6\u00f5\u00fa\u011a\u011b\u00fc\u2020\u00b0\u0118\u00a3\u00a7\u2022\u00b6\u00df\u00ae\u00a9\u2122\u0119\u00a8\u2260\u0123\u012e\u012f\u012a\u2264\u2265\u012b\u0136\u2202\u2211\u0142\u013b\u013c\u013d\u013e\u0139\u013a\u0145\u0146\u0143\u00ac\u221a\u0144\u0147\u2206\u00ab\u00bb\u2026\u00a0\u0148\u0150\u00d5\u0151\u014c\u2013\u2014\u201c\u201d\u2018\u2019\u00f7\u25ca\u014d\u0154\u0155\u0158\u2039\u203a\u0159\u0156\u0157\u0160\u201a\u201e\u0161\u015a\u015b\u00c1\u0164\u0165\u00cd\u017d\u017e\u016a\u00d3\u00d4\u016b\u016e\u00da\u016f\u0170\u0171\u0172\u0173\u00dd\u00fd\u0137\u017b\u0141\u017c\u0122\u02c7";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

const aliases = [ "x-mac-ce" ];

function run_test() {
  testEncodeAliasesInternal();
}
