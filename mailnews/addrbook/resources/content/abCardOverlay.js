/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gEditCard;
var gOnSaveListeners = new Array;
var gOkCallback = null;
var gHideABPicker = false;

function OnLoadNewCard()
{
  InitEditCard();

  gEditCard.card =
    (("arguments" in window) && (window.arguments.length > 0) &&
     (window.arguments[0] instanceof Components.interfaces.nsIAbCard))
    ? window.arguments[0]
    : Components.classes["@mozilla.org/addressbook/cardproperty;1"]
                .createInstance(Components.interfaces.nsIAbCard);
  gEditCard.titleProperty = "newCardTitle";
  gEditCard.selectedAB = "";

  if ("arguments" in window && window.arguments[0])
  {
    gEditCard.selectedAB = kPersonalAddressbookURI;

    if ("selectedAB" in window.arguments[0]) {
      // check if selected ab is a mailing list
      var abURI = window.arguments[0].selectedAB;
      
      var directory = GetDirectoryFromURI(abURI);
      if (directory.isMailList) {
        var parentURI = GetParentDirectoryFromMailingListURI(abURI);
        if (parentURI)
          gEditCard.selectedAB = parentURI;
      }
      else if (directory.operations & directory.opWrite)
        gEditCard.selectedAB = window.arguments[0].selectedAB;
    }

    // we may have been given properties to pre-initialize the window with....
    // we'll fill these in here...
    if ("primaryEmail" in window.arguments[0])
      gEditCard.card.primaryEmail = window.arguments[0].primaryEmail;
    if ("displayName" in window.arguments[0]) {
      gEditCard.card.displayName = window.arguments[0].displayName;
      // if we've got a display name, don't generate
      // a display name (and stomp on the existing display name)
      // when the user types a first or last name
      if (gEditCard.card.displayName.length)
        gEditCard.generateDisplayName = false;
    }
    if ("aimScreenName" in window.arguments[0])
      gEditCard.card.aimScreenName = window.arguments[0].aimScreenName;
    
    if ("okCallback" in window.arguments[0])
      gOkCallback = window.arguments[0].okCallback;

    if ("escapedVCardStr" in window.arguments[0]) {
      // hide non vcard values
      HideNonVcardFields();
      var addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);
      gEditCard.card =
        addressbook.escapedVCardToAbCard(window.arguments[0].escapedVCardStr);
    }

    if ("titleProperty" in window.arguments[0])
      gEditCard.titleProperty = window.arguments[0].titleProperty;
    
    if ("hideABPicker" in window.arguments[0])
      gHideABPicker = window.arguments[0].hideABPicker;
  }

  // set popup with address book names
  var abPopup = document.getElementById('abPopup');
  abPopup.value = gEditCard.selectedAB || kPersonalAddressbookURI;

  if (gHideABPicker && abPopup) {
    abPopup.hidden = true;
    var abPopupLabel = document.getElementById("abPopupLabel");
    abPopupLabel.hidden = true;
  }

  SetCardDialogTitle(gEditCard.card.displayName);
    
  GetCardValues(gEditCard.card, document);

  // FIX ME - looks like we need to focus on both the text field and the tab widget
  // probably need to do the same in the addressing widget

  // focus on first or last name based on the pref
  var focus = document.getElementById(gEditCard.displayLastNameFirst
                                      ? "LastName" : "FirstName");
  if ( focus ) {
    // XXX Using the setTimeout hack until bug 103197 is fixed
    setTimeout( function(firstTextBox) { firstTextBox.focus(); }, 0, focus );
  }
  moveToAlertPosition();
}

// find the index in addressLists for the card that is being saved.
function findCardIndex(directory)
{
  var index = -1;
  var listCardsCount = directory.addressLists.Count();
  for ( var i = 0;  i < listCardsCount; i++ ) {
    var card = directory.addressLists.QueryElementAt(i, Components.interfaces.nsIAbCard);
    if (gEditCard.card.equals(card)) {
      index = i;
      break;
    }
  }
  return index;
}

function EditCardOKButton()
{

  // See if this card is in any mailing list
  // if so then we need to update the addresslists of those mailing lists
  var index = -1;
  var directory = GetDirectoryFromURI(gEditCard.abURI);

  // if the directory is a mailing list we need to search all the mailing lists
  // in the parent directory if the card exists.
  if (directory.isMailList) {
    var parentURI = GetParentDirectoryFromMailingListURI(gEditCard.abURI);
    directory = GetDirectoryFromURI(parentURI);
  }

  var listDirectoriesCount = directory.addressLists.Count();
  var foundDirectories = new Array();
  var foundDirectoriesCount = 0;
  var i;
  // create a list of mailing lists and the index where the card is at.
  for ( i=0;  i < listDirectoriesCount; i++ ) {
    var subdirectory = directory.addressLists.QueryElementAt(i, Components.interfaces.nsIAbDirectory);
    index = findCardIndex(subdirectory);
    if (index > -1)
    {
      foundDirectories[foundDirectoriesCount] = {directory:subdirectory, index:index};
      foundDirectoriesCount++;
    }
  }
  
  SetCardValues(gEditCard.card, document);

  gEditCard.card.editCardToDatabase(gEditCard.abURI);
  
  for (i=0; i<foundDirectoriesCount; i++) {
      // Update the addressLists item for this card
      foundDirectories[i].directory.addressLists.
              SetElementAt(foundDirectories[i].index, gEditCard.card);
  }
  NotifySaveListeners();

  // callback to allow caller to update
  if (gOkCallback)
    gOkCallback();

  return true;  // close the window
}

function OnLoadEditCard()
{
  InitEditCard();

  gEditCard.titleProperty = "editCardTitle";

  if (window.arguments && window.arguments[0])
  {
    if ( window.arguments[0].card )
      gEditCard.card = window.arguments[0].card;
    if ( window.arguments[0].okCallback )
      gOkCallback = window.arguments[0].okCallback;
    if ( window.arguments[0].abURI )
      gEditCard.abURI = window.arguments[0].abURI;
  }

  // set global state variables
  // if first or last name entered, disable generateDisplayName
  if (gEditCard.generateDisplayName &&
      (gEditCard.card.firstName.length +
       gEditCard.card.lastName.length +
       gEditCard.card.displayName.length > 0))
  {
    gEditCard.generateDisplayName = false;
  }

  GetCardValues(gEditCard.card, document);

  SetCardDialogTitle(gEditCard.card.displayName);

  // check if selectedAB is a writeable
  // if not disable all the fields
  if ("arguments" in window && window.arguments[0])
  {
    if ("abURI" in window.arguments[0]) {
      var abURI = window.arguments[0].abURI;
      var directory = GetDirectoryFromURI(abURI);

      if (!(directory.operations & directory.opWrite)) 
      {
        var disableElements = document.getElementsByAttribute("disableforreadonly", "true");
        for (var i=0; i<disableElements.length; i++)
          disableElements[i].setAttribute('disabled', 'true');

        document.documentElement.buttons = "accept";
        document.documentElement.removeAttribute("ondialogaccept");
      }
    }
  }
  
}

// this is used by people who extend the ab card dialog
// like Netscape does for screenname
function RegisterSaveListener(func)
{
  var length = gOnSaveListeners.length;
  gOnSaveListeners[length] = func;
}

// this is used by people who extend the ab card dialog
// like Netscape does for screenname
function NotifySaveListeners()
{
  for ( var i = 0; i < gOnSaveListeners.length; i++ )
    gOnSaveListeners[i]();

  if (gOnSaveListeners.length) {
    // the save listeners might have tweaked the card
    // in which case we need to commit it.
    gEditCard.card.editCardToDatabase(gEditCard.abURI);
  }
}

function InitPhoneticFields()
{
  var showPhoneticFields =
        gPrefs.getComplexValue("mail.addr_book.show_phonetic_fields", 
                               Components.interfaces.nsIPrefLocalizedString).data;

  // hide phonetic fields if indicated by the pref
  if (showPhoneticFields == "true")
  {
    var element = document.getElementById("PhoneticLastName");
    element.setAttribute("hidden", "false");
    element = document.getElementById("PhoneticLabel1");
    element.setAttribute("hidden", "false");
    element = document.getElementById("PhoneticSpacer1");
    element.setAttribute("hidden", "false");

    element = document.getElementById("PhoneticFirstName");
    element.setAttribute("hidden", "false");
    element = document.getElementById("PhoneticLabel2");
    element.setAttribute("hidden", "false");
    element = document.getElementById("PhoneticSpacer2");
    element.setAttribute("hidden", "false");
  }
}

function InitEditCard()
{
  InitPhoneticFields();

  InitCommonJS();
  // Create gEditCard object that contains global variables for the current js
  //   file.
  gEditCard = new Object;

  gEditCard.prefs = gPrefs;

  // get specific prefs that gEditCard will need
  try {
    var displayLastNameFirst =
        gPrefs.getComplexValue("mail.addr_book.displayName.lastnamefirst", 
                               Components.interfaces.nsIPrefLocalizedString).data;
    gEditCard.displayLastNameFirst = (displayLastNameFirst == "true");
    gEditCard.generateDisplayName =
      gPrefs.getBoolPref("mail.addr_book.displayName.autoGeneration");
  }
  catch (ex) {
    dump("ex: failed to get pref" + ex + "\n");
  }
}

function NewCardOKButton()
{
  if (gOkCallback)
  {
    SetCardValues(gEditCard.card, document);
    var addressbook = Components.classes["@mozilla.org/addressbook;1"].createInstance(Components.interfaces.nsIAddressBook);
    gOkCallback(addressbook.abCardToEscapedVCard(gEditCard.card));
    return true;  // close the window
  }

  var popup = document.getElementById('abPopup');
  if ( popup )
  {
    var uri = popup.getAttribute('value');

    // FIX ME - hack to avoid crashing if no ab selected because of blank option bug from template
    // should be able to just remove this if we are not seeing blank lines in the ab popup
    if ( !uri )
      return false;  // don't close window
    // -----

    if (gEditCard.card)
    {
      SetCardValues(gEditCard.card, document);

      var directory = GetDirectoryFromURI(uri);

      // replace gEditCard.card with the card we added
      // so that save listeners can get / set attributes on
      // the card that got created.
      var addedCard = directory.addCard(gEditCard.card);
      gEditCard.card = addedCard;
      NotifySaveListeners();
    }
  }

  return true;  // close the window
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
    doc.getElementById('ScreenName').value = cardproperty.aimScreenName;

    var popup = document.getElementById('PreferMailFormatPopup');
    if ( popup )
      popup.value = cardproperty.preferMailFormat;

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
    doc.getElementById('WebPage2').value = cardproperty.webPage2;

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

    // get phonetic fields if exist
    try {
      doc.getElementById('PhoneticFirstName').value = cardproperty.phoneticFirstName;
      doc.getElementById('PhoneticLastName').value = cardproperty.phoneticLastName;
    }
    catch (ex) {}
  }
}

// when the ab card dialog is being loaded to show a vCard,
// hide the fields which aren't supported
// by vCard so the user does not try to edit them.
function HideNonVcardFields()
{
  document.getElementById('nickNameContainer').collapsed = true;
  document.getElementById('secondaryEmailContainer').collapsed = true;
  document.getElementById('screenNameContainer').collapsed = true;
  document.getElementById('homeAddressGroup').collapsed = true;
  document.getElementById('customFields').collapsed = true;

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
    cardproperty.aimScreenName = doc.getElementById('ScreenName').value;

    var popup = document.getElementById('PreferMailFormatPopup');
    if ( popup )
      cardproperty.preferMailFormat = popup.value;

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
    cardproperty.webPage2 = CleanUpWebPage(doc.getElementById('WebPage2').value);

    cardproperty.jobTitle = doc.getElementById('JobTitle').value;
    cardproperty.department = doc.getElementById('Department').value;
    cardproperty.company = doc.getElementById('Company').value;
    cardproperty.workAddress = doc.getElementById('WorkAddress').value;
    cardproperty.workAddress2 = doc.getElementById('WorkAddress2').value;
    cardproperty.workCity = doc.getElementById('WorkCity').value;
    cardproperty.workState = doc.getElementById('WorkState').value;
    cardproperty.workZipCode = doc.getElementById('WorkZipCode').value;
    cardproperty.workCountry = doc.getElementById('WorkCountry').value;
    cardproperty.webPage1 = CleanUpWebPage(doc.getElementById('WebPage1').value);

    cardproperty.custom1 = doc.getElementById('Custom1').value;
    cardproperty.custom2 = doc.getElementById('Custom2').value;
    cardproperty.custom3 = doc.getElementById('Custom3').value;
    cardproperty.custom4 = doc.getElementById('Custom4').value;
    cardproperty.notes = doc.getElementById('Notes').value;

    // set phonetic fields if exist
    try {
      cardproperty.phoneticFirstName = doc.getElementById('PhoneticFirstName').value;
      cardproperty.phoneticLastName = doc.getElementById('PhoneticLastName').value;
    }
    catch (ex) {}
  }
}

function CleanUpWebPage(webPage)
{
  // no :// yet so we should add something
  if ( webPage.length && webPage.search("://") == -1 )
  {
    // check for missing / on http://
    if ( webPage.substr(0, 6) == "http:/" )
      return( "http://" + webPage.substr(6) );
    else
      return( "http://" + webPage );
  }
  else
    return(webPage);
}

function GenerateDisplayName()
{
  if (gEditCard.generateDisplayName)
  {
    var displayName;

    var firstNameField = document.getElementById('FirstName');
    var lastNameField = document.getElementById('LastName');
    var displayNameField = document.getElementById('DisplayName');

    if (lastNameField.value && firstNameField.value) {
      displayName = (gEditCard.displayLastNameFirst)
        ? gAddressBookBundle.getFormattedString("lastFirstFormat", [lastNameField.value, firstNameField.value])
        : gAddressBookBundle.getFormattedString("firstLastFormat", [firstNameField.value, lastNameField.value]);
    }
    else {
      // one (or both) of these is empty, so this works.
      displayName = firstNameField.value + lastNameField.value;
    }

    displayNameField.value = displayName;

    SetCardDialogTitle(displayName);
  }
}

function DisplayNameChanged()
{
  // turn off generateDisplayName if the user changes the display name
  gEditCard.generateDisplayName = false;

  SetCardDialogTitle(document.getElementById("DisplayName").value);
}

function SetCardDialogTitle(displayName)
{
  document.title = displayName
    ? gAddressBookBundle.getFormattedString(gEditCard.titleProperty + "WithDisplayName", [displayName])
    : gAddressBookBundle.getString(gEditCard.titleProperty);
}
