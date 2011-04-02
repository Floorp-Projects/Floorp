// Tests conversion from UTF-7 to Unicode. The conversion should fail!

load('CharsetConversionTests.js');
	
const inString = "+LGI--+ITIipSIp- +AocCeQ-oddns +Ad0CjQ- s+ATECZQKH- p+AlAB3QJ5- u+AlACVA- no+Ao4- +Al8-I";
    
const expectedString = "+LGI--+ITIipSIp- +AocCeQ-oddns +Ad0CjQ- s+ATECZQKH- p+AlAB3QJ5- u+AlACVA- no+Ao4- +Al8-I";

const aliases = [ "UTF-7", "utf-7", "x-unicode-2-0-utf-7", "unicode-2-0-utf-7",
		  "unicode-1-1-utf-7", "csunicode11utf7" ];

function run_test() {
  testDecodeAliases();
}
