function EditCardOKButton()
{
	var cardproperty = Components.classes["component://netscape/addressbook/cardproperty"].createInstance();
	cardproperty = cardproperty.QueryInterface(Components.interfaces.nsIAbCard);
	//var addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	//addressbook = addressbook.QueryInterface(Components.interfaces.nsIAddressBook);
	dump("cardproperty = " + cardproperty + "\n");

	if (cardproperty)
	{
		cardproperty.FirstName = document.getElementById('FirstName').value;
		cardproperty.LastName = document.getElementById('LastName').value;
		cardproperty.DisplayName = document.getElementById('DisplayName').value;
		cardproperty.NickName = document.getElementById('NickName').value;
		
		cardproperty.PrimaryEmail = document.getElementById('PrimaryEmail').value;
		cardproperty.SecondEmail = document.getElementById('SecondEmail').value;
		//cardproperty.SendPlainText = document.getElementById('SendPlainText').value;
		
		cardproperty.WorkPhone = document.getElementById('WorkPhone').value;
		cardproperty.HomePhone = document.getElementById('HomePhone').value;
		cardproperty.FaxNumber = document.getElementById('FaxNumber').value;
		cardproperty.PagerNumber = document.getElementById('PagerNumber').value;
		cardproperty.CellularNumber = document.getElementById('CellularNumber').value;

		cardproperty.HomeAddress = document.getElementById('HomeAddress').value;
		cardproperty.HomeAddress2 = document.getElementById('HomeAddress2').value;
		cardproperty.HomeCity = document.getElementById('HomeCity').value;
		cardproperty.HomeState = document.getElementById('HomeState').value;
		cardproperty.HomeZipCode = document.getElementById('HomeZipCode').value;
		cardproperty.HomeCountry = document.getElementById('HomeCountry').value;

		cardproperty.JobTitle = document.getElementById('JobTitle').value;
		cardproperty.Department = document.getElementById('Department').value;
		cardproperty.Company = document.getElementById('Company').value;
		cardproperty.WorkAddress = document.getElementById('WorkAddress').value;
		cardproperty.WorkAddress2 = document.getElementById('WorkAddress2').value;
		cardproperty.WorkCity = document.getElementById('WorkCity').value;
		cardproperty.WorkState = document.getElementById('WorkState').value;
		cardproperty.WorkZipCode = document.getElementById('WorkZipCode').value;
		cardproperty.WorkCountry = document.getElementById('WorkCountry').value;

		cardproperty.WebPage1 = document.getElementById('WebPage1').value;

		cardproperty.Custom1 = document.getElementById('Custom1').value;
		cardproperty.Custom2 = document.getElementById('Custom2').value;
		cardproperty.Custom3 = document.getElementById('Custom3').value;
		cardproperty.Custom4 = document.getElementById('Custom4').value;
		cardproperty.Notes = document.getElementById('Notes').value;

		cardproperty.AddCardToDatabase();
//		addressbook.NewaCard();
	}
	top.window.close();
}


function EditCardCancelButton()
{
	top.window.close();
}

