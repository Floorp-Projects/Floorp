// Tests conversion from Unicode to armscii-8
	
load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u0587\u0589\u00bb\u00ab\u2014\u055d\u058a\u2026\u055c\u055b\u055e\u0531\u0561\u0532\u0562\u0533\u0563\u0534\u0564\u0535\u0565\u0536\u0566\u0537\u0567\u0538\u0568\u0539\u0569\u053a\u056a\u053b\u056b\u053c\u056c\u053d\u056d\u053e\u056e\u053f\u056f\u0540\u0570\u0541\u0571\u0542\u0572\u0543\u0573\u0544\u0574\u0545\u0575\u0546\u0576\u0547\u0577\u0548\u0578\u0549\u0579\u054a\u057a\u054b\u057b\u054c\u057c\u054d\u057d\u054e\u057e\u054f\u057f\u0550\u0580\u0551\u0581\u0552\u0582\u0553\u0583\u0554\u0584\u0555\u0585\u0556\u0586\u055a";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~¢£¦§¨ª­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış";

const aliases = ["armscii-8"];

function run_test() {
  testEncodeAliases();
}
