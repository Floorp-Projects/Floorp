var AddressAutoCompleteListener = {
	onAutoCompleteResult: function(aItem, aOriginalString, aMatch) {
		dump("aItem = " + aItem + "\n");

		if ( aItem )
		{
			dump("value = " + aItem.value + "\n");
			dump("aOriginalString = " + aOriginalString + "\n");
			dump("aMatch = " + aMatch + "\n");

			aItem.value = aMatch;
		}
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

	ac.autoComplete(inputElement, inputElement.value, AddressAutoCompleteListener);
}
