// Tests conversion from x-mac-devanagari to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x90\x91\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00d7\u2212\u2013\u2014\u2018\u2019\u2026\u2022\u00a9\u00ae\u2122\u0965\u0970\u0901\u0902\u0903\u0905\u0906\u0907\u0908\u0909\u090a\u090b\u090e\u090f\u0910\u090d\u0912\u0913\u0914\u0911\u0915\u0916\u0917\u0918\u0919\u091a\u091b\u091c\u091d\u091e\u091f\u0920\u0921\u0922\u0923\u0924\u0925\u0926\u0927\u0928\u0929\u092a\u092b\u092c\u092d\u092e\u092f\u095f\u0930\u0931\u0932\u0933\u0934\u0935\u0936\u0937\u0938\u0939\u200e\u093e\u093f\u0940\u0941\u0942\u0943\u0946\u0947\u0948\u0945\u094a\u094b\u094c\u0949\u094d\u093c\u0964\u0966\u0967\u0968\u0969\u096a\u096b\u096c\u096d\u096e\u096f";

const aliases = [ "x-mac-devanagari" ];

function run_test() {
  testDecodeAliasesInternal();
}
