// Tests conversion from IBM864 to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9d\x9e\xa0\xa1\xa2\xa3\xa4\xa5\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe";
    
const expectedString = " !\"#$\u066a&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00b0\u00b7\u2219\u221a\u2592\u2500\u2502\u253c\u2524\u252c\u251c\u2534\u2510\u250c\u2514\u2518\u03b2\u221e\u03c6\u00b1\u00bd\u00bc\u2248\u00ab\u00bb\ufef7\ufef8\ufefb\ufefc\u00a0\u00ad\ufe82\u00a3\u00a4\ufe84\u20ac\ufe8e\ufe8f\ufe95\ufe99\u060c\ufe9d\ufea1\ufea5\u0660\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u0669\ufed1\u061b\ufeb1\ufeb5\ufeb9\u061f\u00a2\ufe80\ufe81\ufe83\ufe85\ufeca\ufe8b\ufe8d\ufe91\ufe93\ufe97\ufe9b\ufe9f\ufea3\ufea7\ufea9\ufeab\ufead\ufeaf\ufeb3\ufeb7\ufebb\ufebf\ufec1\ufec5\ufecb\ufecf\u00a6\u00ac\u00f7\u00d7\ufec9\u0640\ufed3\ufed7\ufedb\ufedf\ufee3\ufee7\ufeeb\ufeed\ufeef\ufef3\ufebd\ufecc\ufece\ufecd\ufee1\ufe7d\u0651\ufee5\ufee9\ufeec\ufef0\ufef2\ufed0\ufed5\ufef5\ufef6\ufedd\ufed9\ufef1\u25a0";

// const aliases = [ "IBM864", "ibm864", "cp864", "864", "csibm864", "ibm-864" ];
// Aliases no longer supported for internal encodings in the test harness
const aliases = [ "IBM864" ];

function run_test() {
  testDecodeAliasesInternal();
}
