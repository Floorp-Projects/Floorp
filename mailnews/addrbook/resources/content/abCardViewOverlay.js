var zWork = "Work: ";
var zHome = "Home: ";
var zFax = "Fax: ";
var zCellular = "Cellular: ";
var zPager = "Pager: ";
var zCustom1 = "Custom 1: ";
var zCustom2 = "Custom 2: ";
var zCustom3 = "Custom 3: ";
var zCustom4 = "Custom 4: ";

var rdf;
var cvData;

function OnLoadCardView()
{
	// This should be in an onload for the card view window, but that is not currently working
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var doc = document;
	
	/* data for address book, prefixes: "cvb" = card view box
										"cvh" = crad view header
										"cv"  = card view (normal fields) */
	cvData = new Object;

	// Card View Box
	cvData.CardViewBox		= doc.getElementById("CardViewBox");
	// Title
	cvData.CardTitle		= doc.getElementById("CardTitle");
	// Name section
	cvData.cvbName			= doc.getElementById("cvbName");
	cvData.cvhName			= doc.getElementById("cvhName");
	cvData.cvNickname		= doc.getElementById("cvNickname");
	cvData.cvEmail1			= doc.getElementById("cvEmail1");
	cvData.cvEmail2			= doc.getElementById("cvEmail2");
	// Home section
	cvData.cvbHome			= doc.getElementById("cvbHome");
	cvData.cvhHome			= doc.getElementById("cvhHome");
	cvData.cvHomeAddress	= doc.getElementById("cvHomeAddress");
	cvData.cvHomeAddress2	= doc.getElementById("cvHomeAddress2");
	cvData.cvHomeCityStZip	= doc.getElementById("cvHomeCityStZip");
	cvData.cvHomeCountry	= doc.getElementById("cvHomeCountry");
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
}
	
function DisplayCardViewPane(abNode)
{
	var uri = abNode.getAttribute('id');
	var cardResource = top.rdf.GetResource(uri);
	var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
	
	// FIX ME - this should use a i18n name routine in JS
	var name = card.displayName;
	if ( card.firstName.length + card.lastName.length > 0 )
		name = card.firstName + " " + card.lastName;
	
	var nickname;
	if ( card.nickName )
		nickname = "\"" + card.nickName + "\"";
		
	var data = top.cvData;
	var visible;
	
	// set fields in card view pane

	cvSetNode(data.CardTitle, "Card for " + card.displayName);
	
	// Name section
	cvSetNode(data.cvhName, name);
	cvSetNode(data.cvNickname, nickname);
	cvSetNode(data.cvEmail1, card.primaryEmail);
	cvSetNode(data.cvEmail2, card.secondEmail);
	// Home section
	visible = cvSetNode(data.cvHomeAddress, card.homeAddress);
	visible = cvSetNode(data.cvHomeAddress2, card.homeAddress2) || visible;
	visible = cvSetCityStateZip(data.cvHomeCityStZip, card.homeCity, card.homeState, card.homeZipCode) || visible;
	visible = cvSetNode(data.cvHomeCountry, card.homeCountry) || visible;
	cvSetVisible(data.cvhHome, visible);
	// Other section
	visible = cvSetNodeWithLabel(data.cvCustom1, zCustom1, card.custom1);
	visible = cvSetNodeWithLabel(data.cvCustom2, zCustom2, card.custom2) || visible;
	visible = cvSetNodeWithLabel(data.cvCustom3, zCustom3, card.custom3) || visible;
	visible = cvSetNodeWithLabel(data.cvCustom4, zCustom4, card.custom4) || visible;
	visible = cvSetNode(data.cvNotes, card.notes) || visible;
	cvSetVisible(data.cvhOther, visible);
	// Phone section
	visible = cvSetNodeWithLabel(data.cvPhWork, zWork, card.workPhone);
	visible = cvSetNodeWithLabel(data.cvPhHome, zHome, card.homePhone) || visible;
	visible = cvSetNodeWithLabel(data.cvPhFax, zFax, card.faxNumber) || visible;
	visible = cvSetNodeWithLabel(data.cvPhCellular, zCellular, card.cellularNumber) || visible;
	visible = cvSetNodeWithLabel(data.cvPhPager, zPager, card.pagerNumber) || visible;
	cvSetVisible(data.cvhPhone, visible);
	// Work section
	visible = cvSetNode(data.cvJobTitle, card.jobTitle);
	visible = cvSetNode(data.cvDepartment, card.department) || visible;
	visible = cvSetNode(data.cvCompany, card.company) || visible;
	visible = cvSetNode(data.cvWorkAddress, card.workAddress) || visible;
	visible = cvSetNode(data.cvWorkAddress2, card.workAddress2) || visible;
	visible = cvSetCityStateZip(data.cvWorkCityStZip, card.workCity, card.workState, card.workZipCode) || visible;
	visible = cvSetNode(data.cvWorkCountry, card.workCountry) || visible;
	cvSetVisible(data.cvhWork, visible);

	// make the card view box visible
	cvSetVisible(top.cvData.CardViewBox, true);
}

function ClearCardViewPane()
{
	dump("ClearCardViewPane\n");
	cvSetVisible(top.cvData.CardViewBox, false);

	// HACK - we need to be able to set the entire box or div to display:none when bug fixed
	var data = top.cvData;

	// title
	cvSetVisible(data.CardTitle, false);
	// Name section
	cvSetVisible(data.cvhName, false);
	cvSetVisible(data.cvNickname, false);
	cvSetVisible(data.cvEmail1, false);
	cvSetVisible(data.cvEmail2, false);
	// Home section
	cvSetVisible(data.cvhHome, false);
	cvSetVisible(data.cvHomeAddress, false);
	cvSetVisible(data.cvHomeAddress2, false);
	cvSetVisible(data.cvHomeCityStZip, false);
	cvSetVisible(data.cvHomeCountry, false);
	// Other section
	cvSetVisible(data.cvhOther, false);
	cvSetVisible(data.cvCustom1, false);
	cvSetVisible(data.cvCustom2, false);
	cvSetVisible(data.cvCustom3, false);
	cvSetVisible(data.cvCustom4, false);
	cvSetVisible(data.cvNotes, false);
	// Phone section
	cvSetVisible(data.cvhPhone, false);
	cvSetVisible(data.cvPhWork, false);
	cvSetVisible(data.cvPhHome, false);
	cvSetVisible(data.cvPhFax, false);
	cvSetVisible(data.cvPhCellular, false);
	cvSetVisible(data.cvPhPager, false);
	// Work section
	cvSetVisible(data.cvhWork, false);
	cvSetVisible(data.cvJobTitle, false);
	cvSetVisible(data.cvDepartment, false);
	cvSetVisible(data.cvCompany, false);
	cvSetVisible(data.cvWorkAddress, false);
	cvSetVisible(data.cvWorkAddress2, false);
	cvSetVisible(data.cvWorkCityStZip, false);
	cvSetVisible(data.cvWorkCountry, false);
}

function cvSetNodeWithLabel(node, label, text)
{
	if ( text )
		return cvSetNode(node, label + text);
	else
	{
		cvSetVisible(node, false);
		return false;
	}
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
	node.childNodes[0].nodeValue = text;
	var visible;
	
	if ( text )
		visible = true;
	else
		visible = false;
	
	cvSetVisible(node, visible);
	return visible;
}

function cvSetVisible(node, visible)
{
	if ( visible )
		node.removeAttribute("hide");
	else
		node.setAttribute("hide", "true");
}

