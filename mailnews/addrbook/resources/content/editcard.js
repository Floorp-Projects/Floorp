function EditCardOKButton()
{
	dump("OK Hit\n");

	var cardproperty = Components.classes["component://netscape/addressbook/cardproperty"].createInstance();
	cardproperty = cardproperty.QueryInterface(Components.interfaces.nsIAbCard);
	dump("cardproperty = " + cardproperty + "\n");

	if (cardproperty)
	{
		cardproperty.SetCardValue('FirstName', document.getElementById('FirstName').value);
		cardproperty.SetCardValue('LastName', document.getElementById('LastName').value);
		cardproperty.SetCardValue('DisplayName', document.getElementById('DisplayName').value);
		cardproperty.SetCardValue('NickName', document.getElementById('NickName').value);
		cardproperty.SetCardValue('PrimaryEmail', document.getElementById('PrimaryEmail').value);
		cardproperty.SetCardValue('SecondEmail', document.getElementById('SecondEmail').value);
		cardproperty.SetCardValue('WorkPhone', document.getElementById('WorkPhone').value);
		cardproperty.SetCardValue('HomePhone', document.getElementById('HomePhone').value);
		cardproperty.SetCardValue('FaxNumber', document.getElementById('FaxNumber').value);
		cardproperty.SetCardValue('PagerNumber', document.getElementById('PagerNumber').value);
		cardproperty.SetCardValue('CellularNumber', document.getElementById('CellularNumber').value);

		cardproperty.AddCardToDatabase();
	}
	top.window.close();
}


function EditCardCancelButton()
{
	dump("Cancel Hit\n");
	top.window.close();
}

