var AddressAutoCompleteListener = {
	OnAutoCompleteResult: function(doc_id, aOriginalString, aMatch) {
		dump("textId = " + doc_id + "\n");

		var field = document.getElementById(doc_id);

		dump("value = " + field.value + "\n");
		dump("aOriginalString = " + aOriginalString + "\n");
		dump("aMatch = " + aMatch + "\n");

		field.value = aMatch;
	}
};

function AutoCompleteAddress(select_doc_id, doc_id)
{
	dump("select_doc_id = " + select_doc_id + "\n");
	dump("doc_id = " + doc_id + "\n");

        var select = document.getElementById(select_doc_id);
	dump("select = " + select + "\n");
	dump("select.value = " + select.value + "\n");

	if (select.value == "addr_newsgroups") {
		dump("don't autocomplete, it's a newsgroup");
		return;
	}
	
	var ac = Components.classes['component://netscape/messenger/autocomplete&type=addrbook'];
        if (ac) {
                ac = ac.getService();
        }       
        if (ac) {
                ac = ac.QueryInterface(Components.interfaces.nsIAutoCompleteSession);
        }

        var field = document.getElementById(doc_id);
	
	ac.AutoComplete(doc_id, field.value, AddressAutoCompleteListener);
}
