// Tests whether characters above 0x7F decode to ASCII characters liable to 
// expose XSS vulnerabilities
load('CharsetConversionTests.js');

function run_test() {
  var failures = false;
  var ccManager = Cc["@mozilla.org/charset-converter-manager;1"]
        .getService(Ci.nsICharsetConverterManager);
  var decodingConverter = CreateScriptableConverter();

  var charsetList = ccManager.getDecoderList();
  var counter = 0;
  while (charsetList.hasMore()) {
    ++counter;
    var charset = charsetList.getNext();
    dump("testing " + counter + " " + charset + "\n");
      
    try {
      decodingConverter.charset = charset;
    } catch(e) {
      dump("Warning: couldn't set decoder charset to " + charset + "\n");
      continue;
    }
    for (var i = 0x80; i < 0x100; ++i) {
      var inString = String.fromCharCode(i);
      var outString;
      try {
	outString = decodingConverter.ConvertToUnicode(inString) +
	                decodingConverter.Finish();
      } catch(e) {
	outString = String.fromCharCode(0xFFFD);
      }
      for (var n = 0; n < outString.length; ++n) {
	var outChar = outString.charAt(n);
	if (outChar == '<' || outChar == '>' || outChar == '/') {
	  dump(charset + " has a problem: " + escape(inString) +
	       " decodes to '" + outString + "'\n");
	  failures = true;
	}
      }
    }
  }
  if (failures) {
    do_throw("test failed\n");
  }
}
