/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jean-Francois Ducarroz <ducarroz@netscape.com>
 * Seth Spitzer <sspitzer@netscape.com>
 * Alec Flett <alecf@netscape.com>
 */

// get the current identity from the UI
var identityElement;

var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);

    
var AddressAutoCompleteListener = {
	onAutoCompleteResult: function(aItem, aOriginalString, aMatch) {
		dump("aItem = " + aItem + "\n");

		if ( aItem )
		{
			dump("value = " + aItem.value + "\n");
			dump("aOriginalString = " + aOriginalString + "\n");
			dump("aMatch = " + aMatch + "\n");
			aItem.value = aMatch;
			aItem.lastValue = aMatch;
		}
	}
};

function AutoCompleteAddress(inputElement)
{
	///////////// (select_doc_id, doc_id)

	dump("inputElement = " + inputElement + "\n");

	//Do we really have to autocomplete?
	if (inputElement.lastValue && inputElement.lastValue == inputElement.value)
		return;

	var row = awGetRowByInputElement(inputElement);	

    var select = awGetPopupElement(row);
	
	dump("select = " + select + "\n");
	dump("select.value = " + select.value + "\n");
	
	if (select.value == "") 
		return;

	if (select.value == "addr_newsgroups") {
		dump("don't autocomplete, it's a newsgroup");
		return;
	}
	
	var ac = Components.classes['@mozilla.org/messenger/autocomplete;1?type=addrbook'];
	if (ac) {
		ac = ac.getService();
	}       
	if (ac) {
		ac = ac.QueryInterface(Components.interfaces.nsIAbAutoCompleteSession);
	}

    if (!identityElement)
        identityElement = document.getElementById("msgIdentity");

    var identity=null;
    
    if (identityElement) {
        idKey = identityElement.value;
        identity = accountManager.getIdentity(idKey);
    }

	ac.autoComplete(identity, inputElement, inputElement.value, AddressAutoCompleteListener);
}
