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

function AutoCompleteAddress(inputElement)
{
	///////////// (select_doc_id, doc_id)

	dump("inputElement = " + inputElement + "\n");

	var row = awGetRowByInputElement(inputElement);	

    var select = awGetPopupElement(row);
	
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

	ac.AutoComplete(inputElement.getAttribute('id'), inputElement.value, AddressAutoCompleteListener);
}
