// Tests conversion from Unicode to x-mac-gurmukhi

load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00d7\u2212\u2013\u2014\u2018\u2019\u2026\u2022\u00a9\u00ae\u2122\u0a71\u0a5c\u0a73\u0a72\u0a74\u0a02\u0a05\u0a06\u0a07\u0a08\u0a09\u0a0a\u0a0f\u0a10\u0a13\u0a14\u0a15\u0a16\u0a17\u0a18\u0a19\u0a1a\u0a1b\u0a1c\u0a1d\u0a1e\u0a1f\u0a20\u0a21\u0a22\u0a23\u0a24\u0a25\u0a26\u0a27\u0a28\u0a2a\u0a2b\u0a2c\u0a2d\u0a2e\u0a2f\u0a30\u0a32\u0a35\uf860\u0a38\u0a39\u200e\u0a3e\u0a3f\u0a40\u0a41\u0a42\u0a47\u0a48\u0a4b\u0a4c\u0a4d\u0a3c\u0964\u0a66\u0a67\u0a68\u0a69\u0a6a\u0a6b\u0a6c\u0a6d\u0a6e\u0a6f";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x90\x91\x92\x93\x94\xa2\xa4\xa5\xa6\xa7\xa8\xa9\xac\xad\xb0\xb1\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc8\xc9\xca\xcb\xcc\xcd\xcf\xd1\xd4\xd5\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xe1\xe2\xe5\xe6\xe8\xe9\xea\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa";

const aliases = [ "x-mac-gurmukhi" ];

function run_test() {
  testEncodeAliasesInternal();
}
