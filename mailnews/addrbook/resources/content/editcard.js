function EditCardOKButton()
{
	dump("OK Hit\n");

	var card = Components.classes["component://netscape/rdf/resource-factory?name=abcard"].createInstance();
	card = card.QueryInterface(Components.interfaces.nsIAbCard);
	dump("card = " + card + "\n");

	if (card)
	{
		card.SetCardValue('firstname', document.getElementById('firstname').value);
		card.SetCardValue('lastname', document.getElementById('lastname').value);
		card.SetCardValue('displayname', document.getElementById('displayname').value);
		card.SetCardValue('nickname', document.getElementById('nickname').value);
		card.SetCardValue('primaryemail', document.getElementById('primaryemail').value);
		card.SetCardValue('secondemail', document.getElementById('secondemail').value);
		card.SetCardValue('workphone', document.getElementById('workphone').value);
		card.SetCardValue('homephone', document.getElementById('homephone').value);
		card.SetCardValue('faxnumber', document.getElementById('faxnumber').value);
		card.SetCardValue('pagernumber', document.getElementById('pagernumber').value);
		card.SetCardValue('cellularnumber', document.getElementById('cellularnumber').value);

		card.AddCardToDatabase();
	}
	top.window.close();
}


function EditCardCancelButton()
{
	dump("Cancel Hit\n");
	top.window.close();
}

