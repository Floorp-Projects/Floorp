// Tests conversion from Unicode to x-mac-gujarati

load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u00d7\u2212\u2013\u2014\u2018\u2019\u2026\u2022\u00a9\u00ae\u2122\u0965\u0a81\u0a82\u0a83\u0a85\u0a86\u0a87\u0a88\u0a89\u0a8a\u0a8b\u0a8f\u0a90\u0a8d\u0a93\u0a94\u0a91\u0a95\u0a96\u0a97\u0a98\u0a99\u0a9a\u0a9b\u0a9c\u0a9d\u0a9e\u0a9f\u0aa0\u0aa1\u0aa2\u0aa3\u0aa4\u0aa5\u0aa6\u0aa7\u0aa8\u0aaa\u0aab\u0aac\u0aad\u0aae\u0aaf\u0ab0\u0ab2\u0ab3\u0ab5\u0ab6\u0ab7\u0ab8\u0ab9\u200e\u0abe\u0abf\u0ac0\u0ac1\u0ac2\u0ac3\u0ac7\u0ac8\u0ac5\u0acb\u0acc\u0ac9\u0acd\u0abc\u0964\u0ae6\u0ae7\u0ae8\u0ae9\u0aea\u0aeb\u0aec\u0aed\u0aee\u0aef";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š¡¢£¤¥¦§¨©ª¬­®°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÈÉÊËÌÍÏÑÒÔÕÖ×ØÙÚÛÜİŞßáâãåæçèéêñòóôõö÷øùú";

const aliases = [ "x-mac-gujarati" ];

function run_test() {
  testEncodeAliases();
}
