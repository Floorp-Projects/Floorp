function run_test()
{
    var failures = false;
    var ccManager =
	Components.classes["@mozilla.org/charset-converter-manager;1"]
	.getService(Components.interfaces.nsICharsetConverterManager);

    var decoderList = ccManager.getDecoderList();
    while (decoderList.hasMore()) {
	var decoder = decoderList.getNext();
	try {
	    var langGroup = ccManager.getCharsetLangGroupRaw(decoder);
	} catch(e) {
	    dump("no langGroup for " + decoder + "\n");
	    failures = true;
	}
    }
    if (failures) {
	do_throw("missing langGroups");
    }
}
