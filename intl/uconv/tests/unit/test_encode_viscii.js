// Tests conversion from Unicode to viscii

load('CharsetConversionTests.js');
	
const inString = "\u1eb2\u1eb4\u1eaa\u1ef6\u1ef8\u1ef4 !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u1ea0\u1eae\u1eb0\u1eb6\u1ea4\u1ea6\u1ea8\u1eac\u1ebc\u1eb8\u1ebe\u1ec0\u1ec2\u1ec4\u1ec6\u1ed0\u1ed2\u1ed4\u1ed6\u1ed8\u1ee2\u1eda\u1edc\u1ede\u1eca\u1ece\u1ecc\u1ec8\u1ee6\u0168\u1ee4\u1ef2\u00d5\u1eaf\u1eb1\u1eb7\u1ea5\u1ea7\u1ea9\u1ead\u1ebd\u1eb9\u1ebf\u1ec1\u1ec3\u1ec5\u1ec7\u1ed1\u1ed3\u1ed5\u1ed7\u1ee0\u01a0\u1ed9\u1edd\u1edf\u1ecb\u1ef0\u1ee8\u1eea\u1eec\u01a1\u1edb\u01af\u00c0\u00c1\u00c2\u00c3\u1ea2\u0102\u1eb3\u1eb5\u00c8\u00c9\u00ca\u1eba\u00cc\u00cd\u0128\u1ef3\u0110\u1ee9\u00d2\u00d3\u00d4\u1ea1\u1ef7\u1eeb\u1eed\u00d9\u00da\u1ef9\u1ef5\u00dd\u1ee1\u01b0\u00e0\u00e1\u00e2\u00e3\u1ea3\u0103\u1eef\u1eab\u00e8\u00e9\u00ea\u1ebb\u00ec\u00ed\u0129\u1ec9\u0111\u1ef1\u00f2\u00f3\u00f4\u00f5\u1ecf\u1ecd\u1ee5\u00f9\u00fa\u0169\u1ee7\u00fd\u1ee3\u1eee";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";

const aliases = [ "VISCII", "viscii", "csviscii" ];

function run_test() {
  testEncodeAliases();
}
