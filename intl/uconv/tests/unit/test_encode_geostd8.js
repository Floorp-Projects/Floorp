// Tests conversion from Unicode to geostd8
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u20ac\u201a\u201e\u2026\u2020\u2021\u2030\u2039\u2018\u2019\u201c\u201d\u2022\u2013\u2014\u203a\u00a0\u00a1\u00a2\u00a3\u00a4\u00a5\u00a6\u00a7\u00a8\u00a9\u00aa\u00ab\u00ac\u00ad\u00ae\u00af\u00b0\u00b1\u00b2\u00b3\u00b4\u00b5\u00b6\u00b7\u00b8\u00b9\u00ba\u00bb\u00bc\u00bd\u00be\u00bf\u10d0\u10d1\u10d2\u10d3\u10d4\u10d5\u10d6\u10f1\u10d7\u10d8\u10d9\u10da\u10db\u10dc\u10f2\u10dd\u10de\u10df\u10e0\u10e1\u10e2\u10f3\u10e3\u10e4\u10e5\u10e6\u10e7\u10e8\u10e9\u10ea\u10eb\u10ec\u10ed\u10ee\u10f4\u10ef\u10f0\u10f5\u2116";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x82\x84\x85\x86\x87\x89\x8b\x91\x92\x93\x94\x95\x96\x97\x9b\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xfd";

const aliases = ["GEOSTD8"];

function run_test() {
  testEncodeAliases();
}
