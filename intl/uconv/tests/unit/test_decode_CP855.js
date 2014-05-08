// Tests conversion from IBM855 to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u0452\u0402\u0453\u0403\u0451\u0401\u0454\u0404\u0455\u0405\u0456\u0406\u0457\u0407\u0458\u0408\u0459\u0409\u045a\u040a\u045b\u040b\u045c\u040c\u045e\u040e\u045f\u040f\u044e\u042e\u044a\u042a\u0430\u0410\u0431\u0411\u0446\u0426\u0434\u0414\u0435\u0415\u0444\u0424\u0433\u0413\u00ab\u00bb\u2591\u2592\u2593\u2502\u2524\u0445\u0425\u0438\u0418\u2563\u2551\u2557\u255d\u0439\u0419\u2510\u2514\u2534\u252c\u251c\u2500\u253c\u043a\u041a\u255a\u2554\u2569\u2566\u2560\u2550\u256c\u00a4\u043b\u041b\u043c\u041c\u043d\u041d\u043e\u041e\u043f\u2518\u250c\u2588\u2584\u041f\u044f\u2580\u042f\u0440\u0420\u0441\u0421\u0442\u0422\u0443\u0423\u0436\u0416\u0432\u0412\u044c\u042c\u2116\u00ad\u044b\u042b\u0437\u0417\u0448\u0428\u044d\u042d\u0449\u0429\u0447\u0427\u00a7\u25a0\u00a0";

//const aliases = [ "IBM855", "ibm855", "cp855", "855", "csibm855" ];
const aliases = [ "IBM855" ];

function run_test() {
  testDecodeAliasesInternal();
}
