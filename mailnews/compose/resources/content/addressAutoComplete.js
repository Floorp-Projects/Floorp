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

function AutoCompleteSimple(doc_id)
{
        // get the current text
	dump("doc_id = " + doc_id + "\n");
	
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
