var editCard;

var newCardTitlePrefix = "New Card for ";
var editCardTitlePrefix = "Card for ";
var editCardFirstLastSeparator = " ";
var editCardLastFirstSeparator = ", ";

function OnLoadNewCard()
{
	InitEditCard();

	doSetOKCancel(NewCardOKButton, 0);
	
	editCard.card = 0;
	editCard.okCallback = 0;
	editCard.titlePrefix = newCardTitlePrefix;

	if (window.arguments && window.arguments[0])
	{
		if ( window.arguments[0].selectedAB )
			editCard.selectedAB = window.arguments[0].selectedAB;
	}

	// set popup with address book names
	var abPopup = document.getElementById('abPopup');
	if ( editCard.selectedAB )
		abPopup.value = editCard.selectedAB;
	
	//// FIX ME - looks like we need to focus on both the text field and the tab widget
	//// probably need to do the same in the addressing widget
	
	// focus on first name
	var firstName = document.getElementById('FirstName');
	if ( firstName )
		firstName.focus();
}


function OnLoadEditCard()
{
	InitEditCard();
	
	doSetOKCancel(EditCardOKButton, 0);

	editCard.titlePrefix = editCardTitlePrefix;

	if (window.arguments && window.arguments[0])
	{
		if ( window.arguments[0].card )
			editCard.card = window.arguments[0].card;
		if ( window.arguments[0].okCallback )
			editCard.okCallback = window.arguments[0].okCallback;
		if ( window.arguments[0].abURI )
			editCard.abURI = window.arguments[0].abURI;
	}
			
	// set global state variables
	// if first or last name entered, disable generateDisplayName
	if ( editCard.generateDisplayName && (editCard.card.FirstName.length +
										  editCard.card.LastName.length +
										  editCard.card.DisplayName.length > 0) )
	{
		editCard.generateDisplayName = false;
	}
	
	GetCardValues(editCard.card, document);
		
	top.window.title = editCard.titlePrefix + editCard.card.DisplayName;
}

function InitEditCard()
{
	// create editCard object that contains global variables for editCard.js
	editCard = new Object;
	
	// get pointer to nsIPref object
	var prefs = Components.classes["component://netscape/preferences"];
	if ( prefs )
	{
		prefs = prefs.getService();
		if ( prefs )
		{
			prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
			editCard.prefs = prefs;
		}
	}
			
	// get specific prefs that editCard will need
	if ( prefs )
	{
		try {
			editCard.displayLastNameFirst = prefs.GetBoolPref("mail.addr_book.lastnamefirst");
		}
		catch (ex) {
			dump("failed to get the mail.addr_book.lastnamefirst pref\n");
		}

		try {
			editCard.generateDisplayName = prefs.GetBoolPref("mail.addr_book.displayName.autoGeneration");
		}
		catch (ex) {
			dump("failed to get the mail.addr_book.displayName.autoGeneration pref\n");
		}
	}
}

function NewCardOKButton()
{
	var popup = document.getElementById('abPopup');
	if ( popup )
	{
		var uri = popup.value;

		var cardproperty = Components.classes["component://netscape/addressbook/cardproperty"].createInstance();
		cardproperty = cardproperty.QueryInterface(Components.interfaces.nsIAbCard);

		if ( cardproperty )
		{
			SetCardValues(cardproperty, document);
		
			cardproperty.AddCardToDatabase(uri);
		}
	}	
	
	return true;	// close the window
}

function EditCardOKButton()
{
	SetCardValues(editCard.card, document);
	
	editCard.card.EditCardToDatabase(editCard.abURI);
	
	// callback to allow caller to update
	if ( editCard.okCallback )
		editCard.okCallback();
	
	return true;	// close the window
}


// Move the data from the cardproperty to the dialog
function GetCardValues(cardproperty, doc)
{
	if ( cardproperty )
	{
		doc.getElementById('FirstName').value = cardproperty.FirstName;
		doc.getElementById('LastName').value = cardproperty.LastName;
		doc.getElementById('DisplayName').value = cardproperty.DisplayName;
		doc.getElementById('NickName').value = cardproperty.NickName;
		
		doc.getElementById('PrimaryEmail').value = cardproperty.PrimaryEmail;
		doc.getElementById('SecondEmail').value = cardproperty.SecondEmail;
		//doc.getElementById('SendPlainText').value = cardproperty.SendPlainText;
		
		doc.getElementById('WorkPhone').value = cardproperty.WorkPhone;
		doc.getElementById('HomePhone').value = cardproperty.HomePhone;
		doc.getElementById('FaxNumber').value = cardproperty.FaxNumber;
		doc.getElementById('PagerNumber').value = cardproperty.PagerNumber;
		doc.getElementById('CellularNumber').value = cardproperty.CellularNumber;

		doc.getElementById('HomeAddress').value = cardproperty.HomeAddress;
		doc.getElementById('HomeAddress2').value = cardproperty.HomeAddress2;
		doc.getElementById('HomeCity').value = cardproperty.HomeCity;
		doc.getElementById('HomeState').value = cardproperty.HomeState;
		doc.getElementById('HomeZipCode').value = cardproperty.HomeZipCode;
		doc.getElementById('HomeCountry').value = cardproperty.HomeCountry;

		doc.getElementById('JobTitle').value = cardproperty.JobTitle;
		doc.getElementById('Department').value = cardproperty.Department;
		doc.getElementById('Company').value = cardproperty.Company;
		doc.getElementById('WorkAddress').value = cardproperty.WorkAddress;
		doc.getElementById('WorkAddress2').value = cardproperty.WorkAddress2;
		doc.getElementById('WorkCity').value = cardproperty.WorkCity;
		doc.getElementById('WorkState').value = cardproperty.WorkState;
		doc.getElementById('WorkZipCode').value = cardproperty.WorkZipCode;
		doc.getElementById('WorkCountry').value = cardproperty.WorkCountry;

		doc.getElementById('WebPage1').value = cardproperty.WebPage1;

		doc.getElementById('Custom1').value = cardproperty.Custom1;
		doc.getElementById('Custom2').value = cardproperty.Custom2;
		doc.getElementById('Custom3').value = cardproperty.Custom3;
		doc.getElementById('Custom4').value = cardproperty.Custom4;
		doc.getElementById('Notes').value = cardproperty.Notes;
	}
}


// Move the data from the dialog to the cardproperty to be stored in the database
function SetCardValues(cardproperty, doc)
{
	if (cardproperty)
	{
		cardproperty.FirstName = doc.getElementById('FirstName').value;
		cardproperty.LastName = doc.getElementById('LastName').value;
		cardproperty.DisplayName = doc.getElementById('DisplayName').value;
		cardproperty.NickName = doc.getElementById('NickName').value;
		
		cardproperty.PrimaryEmail = doc.getElementById('PrimaryEmail').value;
		cardproperty.SecondEmail = doc.getElementById('SecondEmail').value;
		//cardproperty.SendPlainText = doc.getElementById('SendPlainText').value;
		
		cardproperty.WorkPhone = doc.getElementById('WorkPhone').value;
		cardproperty.HomePhone = doc.getElementById('HomePhone').value;
		cardproperty.FaxNumber = doc.getElementById('FaxNumber').value;
		cardproperty.PagerNumber = doc.getElementById('PagerNumber').value;
		cardproperty.CellularNumber = doc.getElementById('CellularNumber').value;

		cardproperty.HomeAddress = doc.getElementById('HomeAddress').value;
		cardproperty.HomeAddress2 = doc.getElementById('HomeAddress2').value;
		cardproperty.HomeCity = doc.getElementById('HomeCity').value;
		cardproperty.HomeState = doc.getElementById('HomeState').value;
		cardproperty.HomeZipCode = doc.getElementById('HomeZipCode').value;
		cardproperty.HomeCountry = doc.getElementById('HomeCountry').value;

		cardproperty.JobTitle = doc.getElementById('JobTitle').value;
		cardproperty.Department = doc.getElementById('Department').value;
		cardproperty.Company = doc.getElementById('Company').value;
		cardproperty.WorkAddress = doc.getElementById('WorkAddress').value;
		cardproperty.WorkAddress2 = doc.getElementById('WorkAddress2').value;
		cardproperty.WorkCity = doc.getElementById('WorkCity').value;
		cardproperty.WorkState = doc.getElementById('WorkState').value;
		cardproperty.WorkZipCode = doc.getElementById('WorkZipCode').value;
		cardproperty.WorkCountry = doc.getElementById('WorkCountry').value;

		cardproperty.WebPage1 = doc.getElementById('WebPage1').value;

		cardproperty.Custom1 = doc.getElementById('Custom1').value;
		cardproperty.Custom2 = doc.getElementById('Custom2').value;
		cardproperty.Custom3 = doc.getElementById('Custom3').value;
		cardproperty.Custom4 = doc.getElementById('Custom4').value;
		cardproperty.Notes = doc.getElementById('Notes').value;
	}
}


function NewCardCancelButton()
{
	top.window.close();
}

function EditCardCancelButton()
{
	top.window.close();
}

function GenerateDisplayName()
{
	if ( editCard.generateDisplayName )
	{
		var displayName;
		
		var firstNameField = document.getElementById('FirstName');
		var lastNameField = document.getElementById('LastName');
		var displayNameField = document.getElementById('DisplayName');

		/* todo: i18N work todo here */
		/* this used to be XP_GetString(MK_ADDR_FIRST_LAST_SEP) */
		
		/* todo:  mscott says there was a pref in 4.5 that would */
		/* cause GenerateDisplayName() to do nothing.  */
		/* the i18N people need it. */
		/* find the pref and heed it. */

		/* trying to be smart about no using the first last sep */
		/* if first or last is missing */
		/* todo:  is this i18N safe? */
		
		var separator = "";
		if ( lastNameField.value && firstNameField.value )
		{
			if ( editCard.displayLastNameFirst )
			 	separator = editCardLastFirstSeparator;
			else
			 	separator = editCardFirstLastSeparator;
		}
		
		if ( editCard.displayLastNameFirst )
			displayName = lastNameField.value + separator + firstNameField.value;
		else
			displayName = firstNameField.value + separator + lastNameField.value;
			
		displayNameField.value = displayName;
		top.window.title = editCard.titlePrefix + displayName;
	}
}

function DisplayNameChanged()
{
	// turn off generateDisplayName if the user changes the display name
	editCard.generateDisplayName = false;
	
	var title = editCard.titlePrefix + document.getElementById('DisplayName').value;
	if ( top.window.title != title )
		top.window.title = title;
}
