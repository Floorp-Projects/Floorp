// Tests conversion from Unicode to x-viet-vps

load('CharsetConversionTests.js');
	
const inString = "\u1ea0\u1eac\u1eb6\u1eb8\u1ec6\u1eca\u1ecc\u1ed8\u1ee2\u1ee4\u1ef0\u1ef4\u1eaa\u1eee !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00c0\u1ea2\u00c3\u1ea4\u1ea6\u1ea8\u1ecd\u1ed7\u0102\u1ebf\u1ec1\u1ec3\u1ec7\u1eae\u1eb0\u1eb2\u1ebe\u2018\u2019\u1ec0\u1ec2\u1ec4\u1ed0\u1ed2\u1ed4\u1ed6\u00fd\u1ef7\u1ef5\u1eda\u1edc\u1ede\u00a0\u1eaf\u1eb1\u1eb3\u1eb5\u1eb7\u1ee0\u1edb\u00d9\u1edd\u1edf\u1ee1\u0168\u1ee8\u1ee3\u1eea\u1ed5\u1eec\u1ef2\u1ef8\u00cd\u00cc\u1ed9\u1ec8\u0128\u00d3\u1eed\u1eef\u00d2\u1ece\u00d5\u1ef1\u1ea7\u00c1\u00c2\u1ea5\u1ea9\u1eab\u1ead\u0111\u1ebb\u00c9\u00ca\u1eb9\u1ec9\u1ec5\u1ecb\u1ef9\u01af\u1ee6\u1ed3\u1ed1\u00d4\u1ecf\u01a1\u00c8\u1eeb\u1ee9\u00da\u0169\u01b0\u00dd\u1eba\u00df\u00e0\u00e1\u00e2\u00e3\u1ea3\u1ea1\u0103\u00e7\u00e8\u00e9\u00ea\u1ebd\u00ec\u00ed\u00ee\u0129\u1eb4\u0110\u00f2\u00f3\u00f4\u00f5\u00f6\u01a0\u1ee5\u00f9\u00fa\u1ee7\u00fc\u1ef6\u1ebc\u1ef3";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

const aliases = [ "x-viet-vps" ];

function run_test() {
  testEncodeAliases();
}
