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
# The Original Code is Mozilla addressbook.
#
# The Initial Developer of the Original Code is
# Seth Spitzer <sspitzer@netscape.com>
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

var searchSessionContractID = "@mozilla.org/messenger/searchSession;1";
var gSearchSession;

var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
var nsIMsgSearchTerm = Components.interfaces.nsIMsgSearchTerm;
var nsMsgSearchOp = Components.interfaces.nsMsgSearchOp;
var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
var nsIAbDirectory = Components.interfaces.nsIAbDirectory;

var gStatusText;
var gSearchBundle;
var gAddressBookBundle;

var gSearchStopButton;
var gPropertiesButton;
var gComposeButton;
var gSearchPhoneticName = "false";

var gRDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

var gSearchAbViewListener = {
  onSelectionChanged: function() {
  },
  onCountChanged: function(total) {
    if (total == 0)
      var statusText = gAddressBookBundle.getString("noMatchFound");
    else
    {
      if (total == 1)
        var statusText = gAddressBookBundle.getString("matchFound");
      else  
        var statusText = gAddressBookBundle.getFormattedString("matchesFound", [total]);
    }
    gStatusText.setAttribute("label", statusText);
  }
};

function searchOnLoad()
{
  initializeSearchWidgets();
  initializeSearchWindowWidgets();

  gSearchBundle = document.getElementById("bundle_search");
  gAddressBookBundle = document.getElementById("bundle_addressBook");
  gSearchSession = Components.classes[searchSessionContractID].createInstance(Components.interfaces.nsIMsgSearchSession);

  // initialize a flag for phonetic name search
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefBranch = prefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  gSearchPhoneticName =
        prefBranch.getComplexValue("mail.addr_book.show_phonetic_fields", 
                                   Components.interfaces.nsIPrefLocalizedString).data;

  if (window.arguments && window.arguments[0])
    SelectDirectory(window.arguments[0].directory);

  // initialize globals, see abCommon.js, InitCommonJS()
  abList = document.getElementById("abPopup");
  gAbResultsTree = document.getElementById("abResultsTree");

  onMore(null);
}

function searchOnUnload()
{
  CloseAbView();
}

function initializeSearchWindowWidgets()
{
  gSearchStopButton = document.getElementById("search-button");
  gPropertiesButton = document.getElementById("propertiesButton");
  gComposeButton = document.getElementById("composeButton");
  gStatusText = document.getElementById('statusText');
}

function onSearchStop() 
{
}

function onAbSearchReset(event) 
{
  gPropertiesButton.setAttribute("disabled","true");
  gComposeButton.setAttribute("disabled","true");

  CloseAbView();

  onReset(event);
  gStatusText.setAttribute("label", "");
}

function SelectDirectory(aURI) 
{
  var selectedAB = aURI;

  if (!selectedAB)
    selectedAB = kPersonalAddressbookURI;

  // set popup with address book names
  var abPopup = document.getElementById('abPopup');
  if ( abPopup )
    abPopup.value = selectedAB;

  setSearchScope(GetScopeForDirectoryURI(selectedAB));
}

function GetScopeForDirectoryURI(aURI)
{
  var directory = gRDF.GetResource(aURI).QueryInterface(nsIAbDirectory);
  var booleanAnd = gSearchBooleanRadiogroup.selectedItem.value == "and";

  if (directory.isRemote) {
    if (booleanAnd)
      return nsMsgSearchScope.LDAPAnd;
    else 
      return nsMsgSearchScope.LDAP;
  }
  else {
    if (booleanAnd)
      return nsMsgSearchScope.LocalABAnd;
    else 
      return nsMsgSearchScope.LocalAB;
  }
}

function onEnterInSearchTerm()
{
  // on enter
  // if not searching, start the search
  // if searching, stop and then start again
  if (gSearchStopButton.getAttribute("label") == gSearchBundle.getString("labelForSearchButton")) { 
     onSearch(); 
  }
  else {
     onSearchStop();
     onSearch();
  }
}

function onSearch()
{
    gStatusText.setAttribute("label", "");
    gPropertiesButton.setAttribute("disabled","true");
    gComposeButton.setAttribute("disabled","true");

    gSearchSession.clearScopes();

    var currentAbURI = document.getElementById('abPopup').getAttribute('value');
 
    gSearchSession.addDirectoryScopeTerm(GetScopeForDirectoryURI(currentAbURI));
    saveSearchTerms(gSearchSession.searchTerms, gSearchSession);

    var searchUri = currentAbURI + "?(";

    var count = gSearchSession.searchTerms.Count();

    for (var i=0; i<count; i++) {
      var searchTerm = gSearchSession.searchTerms.GetElementAt(i).QueryInterface(nsIMsgSearchTerm);

      // get the "and" / "or" value from the first term
      if (i == 0) {
       if (searchTerm.booleanAnd) 
         searchUri += "and";
       else
         searchUri += "or";
      }

      var attrs;

      switch (searchTerm.attrib) {
       case nsMsgSearchAttrib.Name:
         if (gSearchPhoneticName == "false")
           attrs = ["DisplayName","FirstName","LastName","NickName","_AimScreenName"];
         else
           attrs = ["DisplayName","FirstName","LastName","NickName","_AimScreenName","PhoneticFirstName","PhoneticLastName"];
         break;
       case nsMsgSearchAttrib.DisplayName:
         attrs = ["DisplayName"];
         break;
       case nsMsgSearchAttrib.Email:
         attrs = ["PrimaryEmail"];
         break;
       case nsMsgSearchAttrib.PhoneNumber:
         attrs = ["HomePhone","WorkPhone","FaxNumber","PagerNumber","CellularNumber"]; 
         break;
       case nsMsgSearchAttrib.Organization:
         attrs = ["Company"];
         break;
       case nsMsgSearchAttrib.Department:
         attrs = ["Department"];
         break;
       case nsMsgSearchAttrib.City:
         attrs = ["WorkCity"];
         break;
       case nsMsgSearchAttrib.Street:
         attrs = ["WorkAddress"];
         break;
       case nsMsgSearchAttrib.Nickname:
         attrs = ["NickName"];
         break;
       case nsMsgSearchAttrib.WorkPhone:
         attrs = ["WorkPhone"];
         break;
       case nsMsgSearchAttrib.HomePhone:
         attrs = ["HomePhone"];
         break;
       case nsMsgSearchAttrib.Fax:
         attrs = ["FaxNumber"];
         break;
       case nsMsgSearchAttrib.Pager:
         attrs = ["PagerNumber"];
         break;
       case nsMsgSearchAttrib.Mobile:
         attrs = ["CellularNumber"];
         break;
       case nsMsgSearchAttrib.Title:
         attrs = ["JobTitle"];
         break;
       case nsMsgSearchAttrib.AdditionalEmail:
         attrs = ["SecondEmail"];
         break;
       case nsMsgSearchAttrib.ScreenName:
         attrs = ["_AimScreenName"];
         break;
       default:
         dump("XXX " + searchTerm.attrib + " not a supported search attr!\n");
         attrs = ["DisplayName"];
         break;
      }

      var opStr;

      switch (searchTerm.op) {
      case nsMsgSearchOp.Contains:
        opStr = "c";
        break;
      case nsMsgSearchOp.DoesntContain:
        opStr = "!c";
        break;
      case nsMsgSearchOp.Is:
        opStr = "=";
        break;
      case nsMsgSearchOp.Isnt:
        opStr = "!=";
        break;
      case nsMsgSearchOp.BeginsWith:
        opStr = "bw";
        break;
      case nsMsgSearchOp.EndsWith:
        opStr = "ew";
        break;
      case nsMsgSearchOp.SoundsLike:
        opStr = "~=";
        break;
      default:
        opStr = "c";
        break;
      }

      // currently, we can't do "and" and "or" searches at the same time
      // (it's either all "and"s or all "or"s)
      var max_attrs = attrs.length;

      for (var j=0;j<max_attrs;j++) {
       // append the term(s) to the searchUri
       searchUri += "(" + attrs[j] + "," + opStr + "," + encodeURIComponent(searchTerm.value.str) + ")";
      }
    }

    searchUri += ")";
    SetAbView(searchUri, null, null);
}

// used to toggle functionality for Search/Stop button.
function onSearchButton(event)
{
    if (event.target.label == gSearchBundle.getString("labelForSearchButton"))
        onSearch();
    else
        onSearchStop();
}

function GetAbViewListener()
{
  return gSearchAbViewListener;
}

function onProperties()
{
  AbEditSelectedCard();
}

function onCompose()
{
  AbNewMessage();
}

function AbResultsPaneDoubleClick(card)
{
  AbEditCard(card);
}

function UpdateCardView()
{
  var numSelected = GetNumSelectedCards();

  if (!numSelected) {
    gPropertiesButton.setAttribute("disabled","true");
    gComposeButton.setAttribute("disabled","true");
    return;
  }

  gComposeButton.removeAttribute("disabled");

  if (numSelected == 1) 
    gPropertiesButton.removeAttribute("disabled");
  else
    gPropertiesButton.setAttribute("disabled","true");
}

function onChooseDirectory(event) 
{
    var directoryURI = event.id;
    if (directoryURI) {
        SelectDirectory(directoryURI);
    }
}
