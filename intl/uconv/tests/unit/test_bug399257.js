// Tests encoding of characters below U+0020
load('CharsetConversionTests.js');

const inString = "Hello\u000aWorld";
const expectedString = "Hello\nWorld";

function run_test() {
    var failures = false;
    var ccManager = Cc["@mozilla.org/charset-converter-manager;1"]
        .getService(Ci.nsICharsetConverterManager);
    var encodingConverter = CreateScriptableConverter();

    var charsetList = ccManager.getDecoderList();
    var counter = 0;
    while (charsetList.hasMore()) {
	++counter;
	var charset = charsetList.getNext();

	// exclude known non-ASCII compatible charsets
	if (charset.substr(0, "UTF-16".length) == "UTF-16" ||
	    charset == "x-imap4-modified-utf7") {
	    dump("skipping " + counter + " " + charset + "\n");
	    continue;
	}
        dump("testing " + counter + " " + charset + "\n");

        try {
            encodingConverter.charset = charset;
        } catch(e) {
            dump("Warning: couldn't set encoder charset to " + charset + "\n");
            continue;
        }
        var codepageString = encodingConverter.ConvertFromUnicode(inString) +
            encodingConverter.Finish();
	if (codepageString != expectedString) {
	    dump(charset + " encoding failed\n");
	    for (var i = 0; i < expectedString.length; ++i) {
		if (codepageString.charAt(i) != expectedString.charAt(i)) {
		    dump(i.toString(16) + ": 0x" +
			 codepageString.charCodeAt(i).toString(16) + " != " +
			 expectedString.charCodeAt(i).toString(16) + "\n");
		}
	    }
	    failures = true;
	}
    }
    if (failures) {
	do_throw("test failed\n");
    }
}
