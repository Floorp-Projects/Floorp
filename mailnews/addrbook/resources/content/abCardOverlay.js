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
		
		// FIX ME - hack to avoid crashing if no ab selected because of blank option bug from template
		// should be able to just remove this if we are not seeing blank lines in the ab popup
		if ( !uri )
			return false;  // don't close window
		// -----
		
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
	
	editCard.card.editCardToDatabase(editCard.abURI);
	
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
		doc.getElementById('FirstName').value = cardproperty.firstName;
		doc.getElementById('LastName').value = cardproperty.lastName;
		doc.getElementById('DisplayName').value = cardproperty.displayName;
		doc.getElementById('NickName').value = cardproperty.nickName;
		
		doc.getElementById('PrimaryEmail').value = cardproperty.primaryEmail;
		doc.getElementById('SecondEmail').value = cardproperty.secondEmail;
		//doc.getElementById('SendPlainText').value = cardproperty.sendPlainText;
		
		doc.getElementById('WorkPhone').value = cardproperty.workPhone;
		doc.getElementById('HomePhone').value = cardproperty.homePhone;
		doc.getElementById('FaxNumber').value = cardproperty.faxNumber;
		doc.getElementById('PagerNumber').value = cardproperty.pagerNumber;
		doc.getElementById('CellularNumber').value = cardproperty.cellularNumber;

		doc.getElementById('HomeAddress').value = cardproperty.homeAddress;
		doc.getElementById('HomeAddress2').value = cardproperty.homeAddress2;
		doc.getElementById('HomeCity').value = cardproperty.homeCity;
		doc.getElementById('HomeState').value = cardproperty.homeState;
		doc.getElementById('HomeZipCode').value = cardproperty.homeZipCode;
		doc.getElementById('HomeCountry').value = cardproperty.homeCountry;

		doc.getElementById('JobTitle').value = cardproperty.jobTitle;
		doc.getElementById('Department').value = cardproperty.department;
		doc.getElementById('Company').value = cardproperty.company;
		doc.getElementById('WorkAddress').value = cardproperty.workAddress;
		doc.getElementById('WorkAddress2').value = cardproperty.workAddress2;
		doc.getElementById('WorkCity').value = cardproperty.workCity;
		doc.getElementById('WorkState').value = cardproperty.workState;
		doc.getElementById('WorkZipCode').value = cardproperty.workZipCode;
		doc.getElementById('WorkCountry').value = cardproperty.workCountry;

		doc.getElementById('WebPage1').value = cardproperty.webPage1;

		doc.getElementById('Custom1').value = cardproperty.custom1;
		doc.getElementById('Custom2').value = cardproperty.custom2;
		doc.getElementById('Custom3').value = cardproperty.custom3;
		doc.getElementById('Custom4').value = cardproperty.custom4;
		doc.getElementById('Notes').value = cardproperty.notes;
	}
}


// Move the data from the dialog to the cardproperty to be stored in the database
function SetCardValues(cardproperty, doc)
{
	if (cardproperty)
	{
		cardproperty.firstName = doc.getElementById('FirstName').value;
		cardproperty.lastName = doc.getElementById('LastName').value;
		cardproperty.displayName = doc.getElementById('DisplayName').value;
		cardproperty.nickName = doc.getElementById('NickName').value;
		
		cardproperty.primaryEmail = doc.getElementById('PrimaryEmail').value;
		cardproperty.secondEmail = doc.getElementById('SecondEmail').value;
		//cardproperty.SendPlainText = doc.getElementById('SendPlainText').value;
		
		cardproperty.workPhone = doc.getElementById('WorkPhone').value;
		cardproperty.homePhone = doc.getElementById('HomePhone').value;
		cardproperty.faxNumber = doc.getElementById('FaxNumber').value;
		cardproperty.pagerNumber = doc.getElementById('PagerNumber').value;
		cardproperty.cellularNumber = doc.getElementById('CellularNumber').value;

		cardproperty.homeAddress = doc.getElementById('HomeAddress').value;
		cardproperty.homeAddress2 = doc.getElementById('HomeAddress2').value;
		cardproperty.homeCity = doc.getElementById('HomeCity').value;
		cardproperty.homeState = doc.getElementById('HomeState').value;
		cardproperty.homeZipCode = doc.getElementById('HomeZipCode').value;
		cardproperty.homeCountry = doc.getElementById('HomeCountry').value;

		cardproperty.jobTitle = doc.getElementById('JobTitle').value;
		cardproperty.department = doc.getElementById('Department').value;
		cardproperty.company = doc.getElementById('Company').value;
		cardproperty.workAddress = doc.getElementById('WorkAddress').value;
		cardproperty.workAddress2 = doc.getElementById('WorkAddress2').value;
		cardproperty.workCity = doc.getElementById('WorkCity').value;
		cardproperty.workState = doc.getElementById('WorkState').value;
		cardproperty.workZipCode = doc.getElementById('WorkZipCode').value;
		cardproperty.workCountry = doc.getElementById('WorkCountry').value;

		cardproperty.webPage1 = doc.getElementById('WebPage1').value;

		cardproperty.custom1 = doc.getElementById('Custom1').value;
		cardproperty.custom2 = doc.getElementById('Custom2').value;
		cardproperty.custom3 = doc.getElementById('Custom3').value;
		cardproperty.custom4 = doc.getElementById('Custom4').value;
		cardproperty.notes = doc.getElementById('Notes').value;
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
