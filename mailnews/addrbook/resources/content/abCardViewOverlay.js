/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * Contributor(s):
 * Paul Hangas <hangas@netscape.com>
 * Alec Flett <alecf@netscape.com>
 */

var Bundle = srGetStrBundle("chrome://messenger/locale/addressbook/addressBook.properties")

var zName = Bundle.GetStringFromName("propertyName") + ": ";
var zNickname = Bundle.GetStringFromName("propertyNickname") + ": ";
var zDisplayName = Bundle.GetStringFromName("propertyDisplayName") + ": ";
var zWork = Bundle.GetStringFromName("propertyWork") + ": ";
var zHome = Bundle.GetStringFromName("propertyHome") + ": ";
var zFax = Bundle.GetStringFromName("propertyFax") + ": ";
var zCellular = Bundle.GetStringFromName("propertyCellular") + ": ";
var zPager = Bundle.GetStringFromName("propertyPager") + ": ";
var zCustom1 = Bundle.GetStringFromName("propertyCustom1") + ": ";
var zCustom2 = Bundle.GetStringFromName("propertyCustom2") + ": ";
var zCustom3 = Bundle.GetStringFromName("propertyCustom3") + ": ";
var zCustom4 = Bundle.GetStringFromName("propertyCustom4") + ": ";

var rdf;
var cvData;

function OnLoadCardView()
{
	rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

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
	cvData.cvbName			= doc.getElementById("cvbName");
	cvData.cvhName			= doc.getElementById("cvhName");
	cvData.cvNickname		= doc.getElementById("cvNickname");
	cvData.cvDisplayName	= doc.getElementById("cvDisplayName");
	cvData.cvEmail1			= doc.getElementById("cvEmail1");
	cvData.cvEmail2			= doc.getElementById("cvEmail2");
	// Home section
	cvData.cvbHome			= doc.getElementById("cvbHome");
	cvData.cvhHome			= doc.getElementById("cvhHome");
	cvData.cvHomeAddress	= doc.getElementById("cvHomeAddress");
	cvData.cvHomeAddress2	= doc.getElementById("cvHomeAddress2");
	cvData.cvHomeCityStZip	= doc.getElementById("cvHomeCityStZip");
	cvData.cvHomeCountry	= doc.getElementById("cvHomeCountry");
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
	cvData.cvWorkWebPageBox = doc.getElementById("cvWorkWebPageBox");
	cvData.cvWorkWebPage	= doc.getElementById("cvWorkWebPage");
}
	
function DisplayCardViewPane(abNode)
{
	var uri = abNode.getAttribute('id');
	var cardResource = top.rdf.GetResource(uri);
	var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
	
	// name
	var name;
	var separator = "";
	if ( card.lastName && card.firstName )
	{
		if ( cvPrefs.displayLastNameFirst )
		 	separator = cvPrefs.lastFirstSeparator;
		else
		 	separator = cvPrefs.firstLastSeparator;
	}
	if ( cvPrefs.displayLastNameFirst )
		name = card.lastName + separator + card.firstName;
	else
		name = card.firstName + separator + card.lastName;

	var cardTitle = card.name;
	var nameHeader = name;
	
	if ( !nameHeader )
		nameHeader = zName;
		
	var data = top.cvData;
	var visible;

	// set fields in card view pane

	cvSetNode(data.CardTitle, Bundle.formatStringFromName("viewCardTitle", [ cardTitle], 1));
	
	// Name section
	cvSetNode(data.cvhName, nameHeader);
	cvSetNodeWithLabel(data.cvNickname, zNickname, card.nickName);
	cvSetNodeWithLabel(data.cvDisplayName, zDisplayName, card.displayName);
	cvSetNode(data.cvEmail1, card.primaryEmail);
	cvSetNode(data.cvEmail2, card.secondEmail);
	// Home section
	visible = cvSetNode(data.cvHomeAddress, card.homeAddress);
	visible = cvSetNode(data.cvHomeAddress2, card.homeAddress2) || visible;
	visible = cvSetCityStateZip(data.cvHomeCityStZip, card.homeCity, card.homeState, card.homeZipCode) || visible;
	visible = cvSetNode(data.cvHomeCountry, card.homeCountry) || visible;
	var homeWebPageVisible = cvSetNode(data.cvHomeWebPage, card.webPage2);
    visible = homeWebPageVisible || visible;
	data.cvHomeWebPage.setAttribute('href', card.webPage2);
    cvSetVisible(data.cvHomeWebPageBox, homeWebPageVisible);
	cvSetVisible(data.cvhHome, visible);
	cvSetVisible(data.cvbHome, visible);
	// Other section
	visible = cvSetNodeWithLabel(data.cvCustom1, zCustom1, card.custom1);
	visible = cvSetNodeWithLabel(data.cvCustom2, zCustom2, card.custom2) || visible;
	visible = cvSetNodeWithLabel(data.cvCustom3, zCustom3, card.custom3) || visible;
	visible = cvSetNodeWithLabel(data.cvCustom4, zCustom4, card.custom4) || visible;
	visible = cvSetNode(data.cvNotes, card.notes) || visible;
	cvSetVisible(data.cvhOther, visible);
	cvSetVisible(data.cvbOther, visible);
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
	visible = cvSetNode(data.cvWorkAddress, card.workAddress) || visible;
	visible = cvSetNode(data.cvWorkAddress2, card.workAddress2) || visible;
	visible = cvSetCityStateZip(data.cvWorkCityStZip, card.workCity, card.workState, card.workZipCode) || visible;
	visible = cvSetNode(data.cvWorkCountry, card.workCountry) || visible;
	var workWebPageVisible = cvSetNode(data.cvWorkWebPage, card.webPage1);
    visible = workWebPageVisible || visible;
	data.cvWorkWebPage.setAttribute('href', card.webPage1);
    cvSetVisible(data.cvWorkWebPageBox, workWebPageVisible);
	cvSetVisible(data.cvhWork, visible);
	cvSetVisible(data.cvbWork, visible);

	// make the card view box visible
	cvSetVisible(top.cvData.CardViewBox, true);
}

function ClearCardViewPane()
{
	cvSetVisible(top.cvData.CardViewBox, false);
}

function cvSetNodeWithLabel(node, label, text)
{
	if ( text )
		return cvSetNode(node, label + text);
	else
		return cvSetNode(node, "");
}

function cvSetCityStateZip(node, city, state, zip)
{
	var text;
	
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
		if ( node.childNodes.length == 0 )
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

function cvSetVisible(node, visible)
{
	if ( visible )
		node.removeAttribute("hide");
	else
		node.setAttribute("hide", "true");
}

