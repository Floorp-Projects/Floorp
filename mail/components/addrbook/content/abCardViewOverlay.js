# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Original Author:
#   Paul Hangas <hangas@netscape.com>
#
# Contributor(s):
#   Alec Flett <alecf@netscape.com>
#   Seth Spitzer <sspitzer@netscape.com>

//NOTE: gAddressBookBundle must be defined and set or this Overlay won't work

var gPrefs = Components.classes["@mozilla.org/preferences-service;1"];
gPrefs = gPrefs.getService();
gPrefs = gPrefs.QueryInterface(Components.interfaces.nsIPrefBranch);

var gProfileDirURL;

var gMapItURLFormat = gPrefs.getComplexValue("mail.addr_book.mapit_url.format", 
                                              Components.interfaces.nsIPrefLocalizedString).data;

var gAddrbookSession = Components.classes["@mozilla.org/addressbook/services/session;1"].getService().QueryInterface(Components.interfaces.nsIAddrBookSession);

var gIOService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
var gFileHandler = gIOService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);

var zListName;
var zPrimaryEmail;
var zSecondaryEmail;
var zScreenName;
var zNickname;
var zDisplayName;
var zWork;
var zHome;
var zFax;
var zCellular;
var zPager;
var zCustom1;
var zCustom2;
var zCustom3;
var zCustom4;

var cvData;

function OnLoadCardView()
{
  zPrimaryEmail = gAddressBookBundle.getString("propertyPrimaryEmail");
  zSecondaryEmail = gAddressBookBundle.getString("propertySecondaryEmail");
  zScreenName = gAddressBookBundle.getString("propertyScreenName");
  zNickname = gAddressBookBundle.getString("propertyNickname");
  zDisplayName = gAddressBookBundle.getString("propertyDisplayName");
  zListName = gAddressBookBundle.getString("propertyListName");
  zWork = gAddressBookBundle.getString("propertyWork");
  zHome = gAddressBookBundle.getString("propertyHome");
  zFax = gAddressBookBundle.getString("propertyFax");
  zCellular = gAddressBookBundle.getString("propertyCellular");
  zPager = gAddressBookBundle.getString("propertyPager");
  zCustom1 = gAddressBookBundle.getString("propertyCustom1");
  zCustom2 = gAddressBookBundle.getString("propertyCustom2");
  zCustom3 = gAddressBookBundle.getString("propertyCustom3");
  zCustom4 = gAddressBookBundle.getString("propertyCustom4");

	var doc = document;
	
	/* data for address book, prefixes: "cvb" = card view box
										"cvh" = crad view header
										"cv"  = card view (normal fields) */
	cvData = new Object;

	// Card View Box
	cvData.CardViewBox		= doc.getElementById("CardViewInnerBox");
	// Title
	cvData.CardTitle		= doc.getElementById("CardTitle");
	// Name section
	cvData.cvbContact = doc.getElementById("cvbContact");
	cvData.cvhContact = doc.getElementById("cvhContact");
	cvData.cvNickname		= doc.getElementById("cvNickname");
	cvData.cvDisplayName	= doc.getElementById("cvDisplayName");
	cvData.cvEmail1Box		= doc.getElementById("cvEmail1Box");
	cvData.cvEmail1			= doc.getElementById("cvEmail1");
	cvData.cvScreennameBox		= doc.getElementById("cvScreennameBox");
  cvData.cvAimPresence = doc.getElementById("cvAimPresence");
	cvData.cvScreenname		= doc.getElementById("cvScreenname");
	cvData.cvBuddyIcon              = doc.getElementById("cvBuddyIcon");
	cvData.cvListNameBox		= doc.getElementById("cvListNameBox");
	cvData.cvListName               = doc.getElementById("cvListName");
	cvData.cvEmail2Box		= doc.getElementById("cvEmail2Box");
	cvData.cvEmail2			= doc.getElementById("cvEmail2");
	// Home section
	cvData.cvbHome			= doc.getElementById("cvbHome");
	cvData.cvhHome			= doc.getElementById("cvhHome");
	cvData.cvHomeAddress	= doc.getElementById("cvHomeAddress");
	cvData.cvHomeAddress2	= doc.getElementById("cvHomeAddress2");
	cvData.cvHomeCityStZip	= doc.getElementById("cvHomeCityStZip");
	cvData.cvHomeCountry	= doc.getElementById("cvHomeCountry");
  cvData.cvbHomeMapItBox  = doc.getElementById("cvbHomeMapItBox");
  cvData.cvHomeMapIt = doc.getElementById("cvHomeMapIt");
	cvData.cvHomeWebPageBox = doc.getElementById("cvHomeWebPageBox");
	cvData.cvHomeWebPage	= doc.getElementById("cvHomeWebPage");
	// Other section
	cvData.cvbOther			= doc.getElementById("cvbOther");
	cvData.cvhOther			= doc.getElementById("cvhOther");
	cvData.cvCustom1		= doc.getElementById("cvCustom1");
	cvData.cvCustom2		= doc.getElementById("cvCustom2");
	cvData.cvCustom3		= doc.getElementById("cvCustom3");
	cvData.cvCustom4		= doc.getElementById("cvCustom4");
	cvData.cvNotes			= doc.getElementById("cvNotes");
  // Description section (mailing lists only)
  cvData.cvbDescription			= doc.getElementById("cvbDescription");
	cvData.cvhDescription			= doc.getElementById("cvhDescription");
  cvData.cvDescription			= doc.getElementById("cvDescription");
  // Addresses section (mailing lists only)
  cvData.cvbAddresses			= doc.getElementById("cvbAddresses");
	cvData.cvhAddresses			= doc.getElementById("cvhAddresses");
  cvData.cvAddresses			= doc.getElementById("cvAddresses");
	// Phone section
	cvData.cvbPhone			= doc.getElementById("cvbPhone");
	cvData.cvhPhone			= doc.getElementById("cvhPhone");
	cvData.cvPhWork			= doc.getElementById("cvPhWork");
	cvData.cvPhHome			= doc.getElementById("cvPhHome");
	cvData.cvPhFax			= doc.getElementById("cvPhFax");
	cvData.cvPhCellular		= doc.getElementById("cvPhCellular");
	cvData.cvPhPager		= doc.getElementById("cvPhPager");
	// Work section
	cvData.cvbWork			= doc.getElementById("cvbWork");
	cvData.cvhWork			= doc.getElementById("cvhWork");
	cvData.cvJobTitle		= doc.getElementById("cvJobTitle");
	cvData.cvDepartment		= doc.getElementById("cvDepartment");
	cvData.cvCompany		= doc.getElementById("cvCompany");
	cvData.cvWorkAddress	= doc.getElementById("cvWorkAddress");
	cvData.cvWorkAddress2	= doc.getElementById("cvWorkAddress2");
	cvData.cvWorkCityStZip	= doc.getElementById("cvWorkCityStZip");
	cvData.cvWorkCountry	= doc.getElementById("cvWorkCountry");
  cvData.cvbWorkMapItBox  = doc.getElementById("cvbWorkMapItBox");
  cvData.cvWorkMapIt = doc.getElementById("cvWorkMapIt");
	cvData.cvWorkWebPageBox = doc.getElementById("cvWorkWebPageBox");
	cvData.cvWorkWebPage	= doc.getElementById("cvWorkWebPage");
}
	
// XXX todo
// some similar code (in spirit) already exists, see OnLoadEditList()
// perhaps we could combine and put in abCommon.js?
function GetAddressesFromURI(uri)
{
  var addresses = "";

  var editList = GetDirectoryFromURI(uri);
  var addressList = editList.addressLists;
  if (addressList) {
    var total = addressList.Count();
    if (total > 0)
      addresses = addressList.GetElementAt(0).QueryInterface(Components.interfaces.nsIAbCard).primaryEmail;
    for (var i = 1;  i < total; i++ ) {
      addresses += ", " + addressList.GetElementAt(i).QueryInterface(Components.interfaces.nsIAbCard).primaryEmail;
    }
  }
  return addresses;
}

function GoIM()
{
  LaunchUrl(top.cvData.cvAimPresence.getAttribute("url"));
}

function DisplayCardViewPane(card)
{
  var generatedName = gAddrbookSession.generateNameFromCard(card, gPrefs.getIntPref("mail.addr_book.lastnamefirst"));
		
  var data = top.cvData;
  var visible;

  var titleString;
  if (generatedName == "")
    titleString = card.primaryEmail;  // if no generatedName, use email
  else
    titleString = generatedName;

  // set fields in card view pane
  if (card.isMailList)
    cvSetNode(data.CardTitle, gAddressBookBundle.getFormattedString("viewListTitle", [generatedName]));
  else
    cvSetNode(data.CardTitle, gAddressBookBundle.getFormattedString("viewCardTitle", [titleString]));
	
  // Contact section
  cvSetNodeWithLabel(data.cvNickname, zNickname, card.nickName);

  if (card.isMailList) {
    // email1, display name and screenname always hidden when a mailing list.
    cvSetVisible(data.cvDisplayName, false);
    cvSetVisible(data.cvEmail1Box, false);
    cvSetVisible(data.cvScreennameBox, false);

    visible = HandleLink(data.cvListName, zListName, card.displayName, data.cvListNameBox, "mailto:" + encodeURIComponent(GenerateAddressFromCard(card))) || visible;
  }
  else { 
    // listname always hidden if not a mailing list
    cvSetVisible(data.cvListNameBox, false);

    cvSetNodeWithLabel(data.cvDisplayName, zDisplayName, card.displayName);

    visible = HandleLink(data.cvEmail1, zPrimaryEmail, card.primaryEmail, data.cvEmail1Box, "mailto:" + card.primaryEmail) || visible;
  }

  var goimURL = "aim:goim?screenname=" + card.aimScreenName;
  var hasScreenName = HandleLink(data.cvScreenname, zScreenName, card.aimScreenName, data.cvScreennameBox, goimURL);
  
  if (!hasScreenName || gIOService.offline) {
    data.cvAimPresence.removeAttribute("src");
    data.cvAimPresence.removeAttribute("url");
    data.cvAimPresence.setAttribute("width","0");
  }
  else {
    data.cvAimPresence.setAttribute("src","http://big.oscar.aol.com:80/" + card.aimScreenName + "?on_url=http://ncmail.netscape.com/include/nc/images/online.gif&off_url=http://ncmail.netscape.com/include/nc/images/offline.gif");   
    data.cvAimPresence.setAttribute("url", goimURL);
    data.cvAimPresence.setAttribute("width","16");
  }

  visible = hasScreenName || visible;
  visible = HandleLink(data.cvEmail2, zSecondaryEmail, card.secondEmail, data.cvEmail2Box, "mailto:" + card.secondEmail) || visible;

	// Home section
	visible = cvSetNode(data.cvHomeAddress, card.homeAddress);
	visible = cvSetNode(data.cvHomeAddress2, card.homeAddress2) || visible;
	visible = cvSetCityStateZip(data.cvHomeCityStZip, card.homeCity, card.homeState, card.homeZipCode) || visible;
	visible = cvSetNode(data.cvHomeCountry, card.homeCountry) || visible;
        if (visible) {
          var homeMapItUrl = CreateMapItURL(card.homeAddress, card.homeAddress2, card.homeCity, card.homeState, card.homeZipCode, card.homeCountry);
          if (homeMapItUrl) {
	    cvSetVisible(data.cvbHomeMapItBox, true);
            data.cvHomeMapIt.setAttribute('url', homeMapItUrl);
          }
          else {
	    cvSetVisible(data.cvbHomeMapItBox, false);
          }
        }
        else {
	  cvSetVisible(data.cvbHomeMapItBox, false);
        }

  visible = HandleLink(data.cvHomeWebPage, "", card.webPage2, data.cvHomeWebPageBox, card.webPage2) || visible;

	cvSetVisible(data.cvhHome, visible);
	cvSetVisible(data.cvbHome, visible);
  if (card.isMailList) {
    // Description section
	  visible = cvSetNode(data.cvDescription, card.notes)
  	cvSetVisible(data.cvbDescription, visible);

    // Addresses section
	  visible = cvAddAddressNodes(data.cvAddresses, card.mailListURI);
  	cvSetVisible(data.cvbAddresses, visible);
 
    // Other section, not shown for mailing lists.
    cvSetVisible(data.cvbOther, false);
  }
  else {
	  // Other section
	  visible = cvSetNodeWithLabel(data.cvCustom1, zCustom1, card.custom1);
	  visible = cvSetNodeWithLabel(data.cvCustom2, zCustom2, card.custom2) || visible;
	  visible = cvSetNodeWithLabel(data.cvCustom3, zCustom3, card.custom3) || visible;
	  visible = cvSetNodeWithLabel(data.cvCustom4, zCustom4, card.custom4) || visible;
	  visible = cvSetNode(data.cvNotes, card.notes) || visible;
    visible = setBuddyIcon(card, data.cvBuddyIcon) || visible;

    cvSetVisible(data.cvhOther, visible);
    cvSetVisible(data.cvbOther, visible);

    // hide description section, not show for non-mailing lists
    cvSetVisible(data.cvbDescription, false);

    // hide addresses section, not show for non-mailing lists
    cvSetVisible(data.cvbAddresses, false);
  }

	// Phone section
	visible = cvSetNodeWithLabel(data.cvPhWork, zWork, card.workPhone);
	visible = cvSetNodeWithLabel(data.cvPhHome, zHome, card.homePhone) || visible;
	visible = cvSetNodeWithLabel(data.cvPhFax, zFax, card.faxNumber) || visible;
	visible = cvSetNodeWithLabel(data.cvPhCellular, zCellular, card.cellularNumber) || visible;
	visible = cvSetNodeWithLabel(data.cvPhPager, zPager, card.pagerNumber) || visible;
	cvSetVisible(data.cvhPhone, visible);
	cvSetVisible(data.cvbPhone, visible);
	// Work section
	visible = cvSetNode(data.cvJobTitle, card.jobTitle);
	visible = cvSetNode(data.cvDepartment, card.department) || visible;
	visible = cvSetNode(data.cvCompany, card.company) || visible;
        
        var addressVisible = cvSetNode(data.cvWorkAddress, card.workAddress);
	addressVisible = cvSetNode(data.cvWorkAddress2, card.workAddress2) || addressVisible;
	addressVisible = cvSetCityStateZip(data.cvWorkCityStZip, card.workCity, card.workState, card.workZipCode) || addressVisible;
	addressVisible = cvSetNode(data.cvWorkCountry, card.workCountry) || addressVisible;

        if (addressVisible) {
          var workMapItUrl = CreateMapItURL(card.workAddress, card.workAddress2, card.workCity, card.workState, card.workZipCode, card.workCountry);
          data.cvWorkMapIt.setAttribute('url', workMapItUrl);
          if (workMapItUrl) {
	    cvSetVisible(data.cvbWorkMapItBox, true);
            data.cvWorkMapIt.setAttribute('url', workMapItUrl);
          }
          else {
	    cvSetVisible(data.cvbWorkMapItBox, false);
          }
        }
        else {
	  cvSetVisible(data.cvbWorkMapItBox, false);
        }

        visible = HandleLink(data.cvWorkWebPage, "", card.webPage1, data.cvWorkWebPageBox, card.webPage1) || addressVisible || visible;

	cvSetVisible(data.cvhWork, visible);
	cvSetVisible(data.cvbWork, visible);

	// make the card view box visible
	cvSetVisible(top.cvData.CardViewBox, true);
}

function setBuddyIcon(card, buddyIcon)
{
  try {
    var myScreenName = gPrefs.getCharPref("aim.session.screenname");
    if (myScreenName && card.primaryEmail) {
      if (!gProfileDirURL) {
        // lazily create these file urls, and keep them around
        var dirService = Components.classes["@mozilla.org/file/directory_service;1"]
            .getService(Components.interfaces.nsIProperties);
        var profileDir = dirService.get("ProfD", Components.interfaces.nsIFile);
        gProfileDirURL = gIOService.newFileURI(profileDir);
      }

      // if we did have a buddy icon on disk for this screenname, this would be the file url spec for it
      var iconURLStr = gProfileDirURL.spec + "/NIM/" + myScreenName + "/picture/" + card.aimScreenName + ".gif";

      // check if the file exists
      var file = gFileHandler.getFileFromURLSpec(iconURLStr);
            
      // check if the file exists
      // is this a perf hit?  (how expensive is stat()?)
      if (file.exists()) {
        buddyIcon.setAttribute("src", iconURLStr);
        return true;
      }
    }
  }
  catch (ex) {
    // can get here if no screenname
  }
  
  buddyIcon.setAttribute("src", "");
  return false;
}

function ClearCardViewPane()
{
	cvSetVisible(top.cvData.CardViewBox, false);
}

function cvSetNodeWithLabel(node, label, text)
{
  if (text) {
    if (label)
      return cvSetNode(node, label + ": " + text);
    else
      return cvSetNode(node, text);
  }
  else
    return cvSetNode(node, "");
}

function cvSetCityStateZip(node, city, state, zip)
{
	var text = "";
	
	if ( city )
	{
		text = city;
		if ( state || zip )
			text += ", ";
	}
	if ( state )
		text += state + " ";
	if ( zip )
		text += zip;
	
	return cvSetNode(node, text);
}

function cvSetNode(node, text)
{
	if ( node )
	{
		if ( !node.hasChildNodes() )
		{
			var textNode = document.createTextNode(text);
			node.appendChild(textNode);                   			
		}
		else if ( node.childNodes.length == 1 )
			node.childNodes[0].nodeValue = text;

		var visible;
		
		if ( text )
			visible = true;
		else
			visible = false;
		
		cvSetVisible(node, visible);
	}

	return visible;
}

function cvAddAddressNodes(node, uri)
{
  var visible = false;
	if ( node )
	{
    var displayName = ""; 
    var address = "";

    var editList = GetDirectoryFromURI(uri);
    var addressList = editList.addressLists;
      
    if (addressList) {
      var total = addressList.Count();
      if (total > 0) {
        while (node.hasChildNodes()) {
          node.removeChild(node.lastChild);
        }
        for (i = 0;  i < total; i++ ) {
      		var descNode = document.createElement("description");   
          address = addressList.GetElementAt(i).QueryInterface(Components.interfaces.nsIAbCard).primaryEmail;    
          displayName = addressList.GetElementAt(i).QueryInterface(Components.interfaces.nsIAbCard).displayName;  
          descNode.setAttribute("class", "CardViewLink");
          node.appendChild(descNode);         
          
          var linkNode = document.createElementNS("http://www.w3.org/1999/xhtml", "a");
          linkNode.setAttribute("id", "addr#" + i);
          linkNode.setAttribute("href", "mailto:" + address);
          descNode.appendChild(linkNode);
          
          var textNode = document.createTextNode(displayName + " <" + address + ">");
          linkNode.appendChild(textNode);
        }
			  visible = true;
      }
		}    
		cvSetVisible(node, visible);
	}
	return visible;
}

function cvSetVisible(node, visible)
{
	if ( visible )
		node.removeAttribute("collapsed");
	else
		node.setAttribute("collapsed", "true");
}

function HandleLink(node, label, value, box, link)
{
  var visible = cvSetNodeWithLabel(node, label, value);
  if (visible)
    node.setAttribute('href', link);
  cvSetVisible(box, visible);

  return visible;
}

function MapIt(id)
{
  var button = document.getElementById(id);
  try {
    var messenger = Components.classes["@mozilla.org/messenger;1"].createInstance();
    messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
    messenger.launchExternalURL(button.getAttribute('url'));  
  } catch (ex) {}
}

function CreateMapItURL(address1, address2, city, state, zip, country)
{
  if (!gMapItURLFormat)
    return null;

  var urlFormat = gMapItURLFormat.replace("@A1", encodeURIComponent(address1));
  urlFormat = urlFormat.replace("@A2", encodeURIComponent(address2));
  urlFormat = urlFormat.replace("@CO", encodeURIComponent(country));
  urlFormat = urlFormat.replace("@CI", encodeURIComponent(city));
  urlFormat = urlFormat.replace("@ST", encodeURIComponent(state));
  urlFormat = urlFormat.replace("@ZI", encodeURIComponent(zip));
  
  return urlFormat;
}

// XXX merge with the code in Map It
function openLink(id)
{
  try {
    var messenger = Components.classes["@mozilla.org/messenger;1"].createInstance();
    messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
    messenger.launchExternalURL(document.getElementById(id).getAttribute("href"));  
  } catch (ex) {}

  // return false, so we don't load the href in the addressbook window
  return false;
}

