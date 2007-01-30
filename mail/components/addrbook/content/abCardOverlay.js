# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Seth Spitzer <sspitzer@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const kNonVcardFields =
        ["nickNameContainer", "secondaryEmailContainer", "screenNameContainer",
         "homeAddressGroup", "customFields", "allowRemoteContent"];

const kPhoneticFields =
        ["PhoneticLastName", "PhoneticLabel1", "PhoneticSpacer1",
         "PhoneticFirstName", "PhoneticLabel2", "PhoneticSpacer2"];

// Item is |[dialogField, cardProperty]|.
const kVcardFields =
        [ // Contact > Name
         ["FirstName", "firstName"],
         ["LastName", "lastName"],
         ["DisplayName", "displayName"],
         ["NickName", "nickName"],
          // Contact > Internet
         ["PrimaryEmail", "primaryEmail"],
         ["SecondEmail", "secondEmail"],
         ["ScreenName", "aimScreenName"], // NB: AIM.
          // Contact > Phones
         ["WorkPhone", "workPhone"],
         ["HomePhone", "homePhone"],
         ["FaxNumber", "faxNumber"],
         ["PagerNumber", "pagerNumber"],
         ["CellularNumber", "cellularNumber"],
          // Address > Home
         ["HomeAddress", "homeAddress"],
         ["HomeAddress2", "homeAddress2"],
         ["HomeCity", "homeCity"],
         ["HomeState", "homeState"],
         ["HomeZipCode", "homeZipCode"],
         ["HomeCountry", "homeCountry"],
         ["WebPage2", "webPage2"],
          // Address > Work
         ["JobTitle", "jobTitle"],
         ["Department", "department"],
         ["Company", "company"],
         ["WorkAddress", "workAddress"],
         ["WorkAddress2", "workAddress2"],
         ["WorkCity", "workCity"],
         ["WorkState", "workState"],
         ["WorkZipCode", "workZipCode"],
         ["WorkCountry", "workCountry"],
         ["WebPage1", "webPage1"],
          // Other > (custom)
         ["Custom1", "custom1"],
         ["Custom2", "custom2"],
         ["Custom3", "custom3"],
         ["Custom4", "custom4"],
          // Other > Notes
         ["Notes", "notes"]];

var gEditCard;
var gOnSaveListeners = new Array();
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
    
    if ("allowRemoteContent" in window.arguments[0]) {
      gEditCard.card.allowRemoteContent = window.arguments[0].allowRemoteContent;
      window.arguments[0].allowRemoteContent = false;
    }

    if ("okCallback" in window.arguments[0])
      gOkCallback = window.arguments[0].okCallback;

    if ("escapedVCardStr" in window.arguments[0]) {
      // hide non vcard values
      HideNonVcardFields();
      gEditCard.card =
        Components.classes["@mozilla.org/addressbook;1"]
                  .createInstance(Components.interfaces.nsIAddressBook)
                  .escapedVCardToAbCard(window.arguments[0].escapedVCardStr);
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
    document.getElementById("abPopupLabel").hidden = true;
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

// @Returns The index in addressLists for the card that is being saved;
//          or |-1| if the card is not found.
function findCardIndex(directory)
{
  var i = directory.addressLists.Count();
  while (i-- > 0) {
    var card = directory.addressLists.QueryElementAt(i, Components.interfaces.nsIAbCard);
    if (gEditCard.card.equals(card))
      break;
  }
  return i;
}

function EditCardOKButton()
{
  if (!CheckCardRequiredDataPresence(document))
    return false;  // don't close window

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
  
  CheckAndSetCardValues(gEditCard.card, document, false);

  directory.modifyCard(gEditCard.card);
  
  for (i=0; i < foundDirectoriesCount; i++) {
      // Update the addressLists item for this card
      foundDirectories[i].directory.addressLists.
              SetElementAt(foundDirectories[i].index, gEditCard.card);
  }
                                        
  NotifySaveListeners(directory);

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
        // Set all the editable vcard fields to read only
        for (var i = kVcardFields.length; i-- > 0; )
          document.getElementById(kVcardFields[i][0]).readOnly = true;

        // And the phonetic fields
        document.getElementById(kPhoneticFields[0]).readOnly = true;
        document.getElementById(kPhoneticFields[3]).readOnly = true;

        // Also disable the mail format popup.
        document.getElementById("PreferMailFormatPopup").disabled = true;      

        document.documentElement.buttons = "accept";
        document.documentElement.removeAttribute("ondialogaccept");
      }
      
      // hide  remote content in HTML field for remote directories
      if (directory.isRemote)
        document.getElementById('allowRemoteContent').hidden = true;
    }
  }
}

// this is used by people who extend the ab card dialog
// like Netscape does for screenname
function RegisterSaveListener(func)
{
  gOnSaveListeners[gOnSaveListeners.length] = func;
}

// this is used by people who extend the ab card dialog
// like Netscape does for screenname
function NotifySaveListeners(directory)
{
  if (!gOnSaveListeners.length)
    return;

  for ( var i = 0; i < gOnSaveListeners.length; i++ )
    gOnSaveListeners[i]();

  // the save listeners might have tweaked the card
  // in which case we need to commit it.
  directory.modifyCard(gEditCard.card);
}

function InitPhoneticFields()
{
  var showPhoneticFields =
        gPrefs.getComplexValue("mail.addr_book.show_phonetic_fields", 
                               Components.interfaces.nsIPrefLocalizedString).data;

  // hide phonetic fields if indicated by the pref
  if (showPhoneticFields == "true")
  {
    for (var i = kPhoneticFields.length; i-- > 0; )
      document.getElementById(kPhoneticFields[i]).hidden = false;
  }
}

function InitEditCard()
{
  InitPhoneticFields();

  InitCommonJS();

  // Create gEditCard object that contains global variables for the current js
  //   file.
  gEditCard = new Object();

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
    if (!CheckAndSetCardValues(gEditCard.card, document, true))
      return false;  // don't close window

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
      if (!CheckAndSetCardValues(gEditCard.card, document, true))
        return false;  // don't close window

      // replace gEditCard.card with the card we added
      // so that save listeners can get / set attributes on
      // the card that got created.
      gEditCard.card = GetDirectoryFromURI(uri).addCard(gEditCard.card);
      NotifySaveListeners();
      if ("arguments" in window && window.arguments[0])
        window.arguments[0].allowRemoteContent = gEditCard.card.allowRemoteContent;
    }
  }

  return true;  // close the window
}

// Move the data from the cardproperty to the dialog
function GetCardValues(cardproperty, doc)
{
  if (!cardproperty)
    return;

  for (var i = kVcardFields.length; i-- > 0; )
    doc.getElementById(kVcardFields[i][0]).value =
      cardproperty[kVcardFields[i][1]];

  var popup = document.getElementById("PreferMailFormatPopup");
  if (popup)
    popup.value = cardproperty.preferMailFormat;
    
  var allowRemoteContentEl = document.getElementById("allowRemoteContent");
  if (allowRemoteContentEl)
    allowRemoteContentEl.checked = cardproperty.allowRemoteContent;

  // get phonetic fields if exist
  try {
    doc.getElementById("PhoneticFirstName").value = cardproperty.phoneticFirstName;
    doc.getElementById("PhoneticLastName").value = cardproperty.phoneticLastName;
  }
  catch (ex) {}
}

// when the ab card dialog is being loaded to show a vCard,
// hide the fields which aren't supported
// by vCard so the user does not try to edit them.
function HideNonVcardFields()
{
  for (var i = kNonVcardFields.length; i-- > 0; )
    document.getElementById(kNonVcardFields[i]).collapsed = true;
}

// Move the data from the dialog to the cardproperty to be stored in the database
// @Returns false - Some required data are missing (card values were not set);
//          true - Card values were set, or there is no card to set values on.
function CheckAndSetCardValues(cardproperty, doc, check)
{
  // If requested, check the required data presence.
  if (check && !CheckCardRequiredDataPresence(document))
    return false;

  if (!cardproperty)
    return true;

  for (var i = kVcardFields.length; i-- > 0; )
    cardproperty[kVcardFields[i][1]] =
      doc.getElementById(kVcardFields[i][0]).value;

  var popup = document.getElementById("PreferMailFormatPopup");
  if (popup)
    cardproperty.preferMailFormat = popup.value;
    
  var allowRemoteContentEl = document.getElementById("allowRemoteContent");
  if (allowRemoteContentEl)
    cardproperty.allowRemoteContent = allowRemoteContentEl.checked;
    
  // set phonetic fields if exist
  try {
    cardproperty.phoneticFirstName = doc.getElementById("PhoneticFirstName").value;
    cardproperty.phoneticLastName = doc.getElementById("PhoneticLastName").value;
  }
  catch (ex) {}

  return true;
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

// @Returns false - Some required data are missing;
//          true - All required data are present.
function CheckCardRequiredDataPresence(doc)
{
  // Bug 314995 We require at least one of the following fields to be
  // filled in: email address, first name, last name, display name,
  //            organization (company name).
  var primaryEmail = doc.getElementById("PrimaryEmail");
  if (primaryEmail.textLength == 0 &&
      doc.getElementById("FirstName").textLength == 0 &&
      doc.getElementById("LastName").textLength == 0 &&
      doc.getElementById("DisplayName").textLength == 0 &&
      doc.getElementById("Company").textLength == 0)
  {
    Components
      .classes["@mozilla.org/embedcomp/prompt-service;1"]
      .getService(Components.interfaces.nsIPromptService)
      .alert(
        window,
        gAddressBookBundle.getString("cardRequiredDataMissingTitle"),
        gAddressBookBundle.getString("cardRequiredDataMissingMessage"));

    return false;
  }

  // Simple checks that the primary email should be of the form |user@host|.
  // Note: if the length of the primary email is 0 then we skip the check
  // as some other field must have something as per the check above.
  if (primaryEmail.textLength != 0 && !/.@./.test(primaryEmail.value))
  {
    Components
      .classes["@mozilla.org/embedcomp/prompt-service;1"]
      .getService(Components.interfaces.nsIPromptService)
      .alert(
        window,
        gAddressBookBundle.getString("incorrectEmailAddressFormatTitle"),
        gAddressBookBundle.getString("incorrectEmailAddressFormatMessage"));

    // Focus the dialog field, to help the user.
    document.getElementById("abTabs").selectedIndex = 0;
    primaryEmail.focus();

    return false;
  }

  return true;
}

function GenerateDisplayName()
{
  if (!gEditCard.generateDisplayName)
    return;

  var displayName;

  var firstNameValue = document.getElementById("FirstName").value;
  var lastNameValue = document.getElementById("LastName").value;
  if (lastNameValue && firstNameValue) {
    displayName = (gEditCard.displayLastNameFirst)
      ? gAddressBookBundle.getFormattedString("lastFirstFormat", [lastNameValue, firstNameValue])
      : gAddressBookBundle.getFormattedString("firstLastFormat", [firstNameValue, lastNameValue]);
  }
  else {
    // one (or both) of these is empty, so this works.
    displayName = firstNameValue + lastNameValue;
  }

  document.getElementById("DisplayName").value = displayName;

  SetCardDialogTitle(displayName);
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
