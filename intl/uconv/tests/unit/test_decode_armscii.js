// Tests conversion from armscii-8 to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u0587\u0589)(\u00bb\u00ab\u2014.\u055d,-\u058a\u2026\u055c\u055b\u055e\u0531\u0561\u0532\u0562\u0533\u0563\u0534\u0564\u0535\u0565\u0536\u0566\u0537\u0567\u0538\u0568\u0539\u0569\u053a\u056a\u053b\u056b\u053c\u056c\u053d\u056d\u053e\u056e\u053f\u056f\u0540\u0570\u0541\u0571\u0542\u0572\u0543\u0573\u0544\u0574\u0545\u0575\u0546\u0576\u0547\u0577\u0548\u0578\u0549\u0579\u054a\u057a\u054b\u057b\u054c\u057c\u054d\u057d\u054e\u057e\u054f\u057f\u0550\u0580\u0551\u0581\u0552\u0582\u0553\u0583\u0554\u0584\u0555\u0585\u0556\u0586\u055a";

const aliases = ["armscii-8"];

function run_test() {
  testDecodeAliases();
}
