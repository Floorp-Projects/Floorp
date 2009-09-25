// Tests conversion from IBM855 to Unicode
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹Œ‘’“”•–—˜™š›œŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u0452\u0402\u0453\u0403\u0451\u0401\u0454\u0404\u0455\u0405\u0456\u0406\u0457\u0407\u0458\u0408\u0459\u0409\u045a\u040a\u045b\u040b\u045c\u040c\u045e\u040e\u045f\u040f\u044e\u042e\u044a\u042a\u0430\u0410\u0431\u0411\u0446\u0426\u0434\u0414\u0435\u0415\u0444\u0424\u0433\u0413\u00ab\u00bb\u2591\u2592\u2593\u2502\u2524\u0445\u0425\u0438\u0418\u2563\u2551\u2557\u255d\u0439\u0419\u2510\u2514\u2534\u252c\u251c\u2500\u253c\u043a\u041a\u255a\u2554\u2569\u2566\u2560\u2550\u256c\u00a4\u043b\u041b\u043c\u041c\u043d\u041d\u043e\u041e\u043f\u2518\u250c\u2588\u2584\u041f\u044f\u2580\u042f\u0440\u0420\u0441\u0421\u0442\u0422\u0443\u0423\u0436\u0416\u0432\u0412\u044c\u042c\u2116\u00ad\u044b\u042b\u0437\u0417\u0448\u0428\u044d\u042d\u0449\u0429\u0447\u0427\u00a7\u25a0\u00a0";

const aliases = [ "IBM855", "ibm855", "cp855", "855", "csibm855" ];

function run_test() {
  testDecodeAliases();
}
