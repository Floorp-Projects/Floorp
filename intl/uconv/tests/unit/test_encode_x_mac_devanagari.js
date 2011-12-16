// Tests conversion from Unicode to x-mac-devanagari

load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00d7\u2212\u2013\u2014\u2018\u2019\u2026\u2022\u00a9\u00ae\u2122\u0965\u0970\u0901\u0902\u0903\u0905\u0906\u0907\u0908\u0909\u090a\u090b\u090e\u090f\u0910\u090d\u0912\u0913\u0914\u0911\u0915\u0916\u0917\u0918\u0919\u091a\u091b\u091c\u091d\u091e\u091f\u0920\u0921\u0922\u0923\u0924\u0925\u0926\u0927\u0928\u0929\u092a\u092b\u092c\u092d\u092e\u092f\u095f\u0930\u0931\u0932\u0933\u0934\u0935\u0936\u0937\u0938\u0939\u200e\u093e\u093f\u0940\u0941\u0942\u0943\u0946\u0947\u0948\u0945\u094a\u094b\u094c\u0949\u094d\u093c\u0964\u0966\u0967\u0968\u0969\u096a\u096b\u096c\u096d\u096e\u096f";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‘¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêñòóôõö÷øùú";

const aliases = [ "x-mac-devanagari" ];

function run_test() {
  testEncodeAliases();
}
