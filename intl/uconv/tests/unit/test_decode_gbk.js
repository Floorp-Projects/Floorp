// Tests conversion from gbk to Unicode
// This is a sniff test which doesn't cover the full gbk range: the test string
// includes only the ASCII range and the first 63 double byte characters

load('CharsetConversionTests.js');
	
const inString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    
const expectedString = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u4E02\u4E04\u4E05\u4E06\u4E0F\u4E12\u4E17\u4E1F\u4E20\u4E21\u4E23\u4E26\u4E29\u4E2E\u4E2F\u4E31\u4E33\u4E35\u4E37\u4E3C\u4E40\u4E41\u4E42\u4E44\u4E46\u4E4A\u4E51\u4E55\u4E57\u4E5A\u4E5B\u4E62\u4E63\u4E64\u4E65\u4E67\u4E68\u4E6A\u4E6B\u4E6C\u4E6D\u4E6E\u4E6F\u4E72\u4E74\u4E75\u4E76\u4E77\u4E78\u4E79\u4E7A\u4E7B\u4E7C\u4E7D\u4E7F\u4E80\u4E81\u4E82\u4E83\u4E84\u4E85\u4E87\u4E8A";

const aliases = [ "gbk", "x-gbk" ];

function run_test() {
  testDecodeAliases();
}
