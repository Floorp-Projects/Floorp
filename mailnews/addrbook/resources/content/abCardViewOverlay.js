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
	var name = card.DisplayName;
	if ( card.FirstName.length + card.LastName.length > 0 )
		name = card.FirstName + " " + card.LastName;
	
	var nickname;
	if ( card.NickName )
		nickname = "\"" + card.NickName + "\"";
		
	var data = top.cvData;
	var visible;
	
	// set fields in card view pane
	// FIX ME - waiting for bug fix... cvSetVisible(data.CardViewBox, true);
	
	cvSetNode(data.CardTitle, "Card for " + card.DisplayName);
	
	// FIX ME!
	// Code needs to be fixed to make the entire box visible or not.  Current hack just hides
	// the header of the section that should be visible.
	
	// Name section
	cvSetNode(data.cvhName, name);
	cvSetNode(data.cvNickname, nickname);
	cvSetNode(data.cvEmail1, card.PrimaryEmail);
	cvSetNode(data.cvEmail2, card.SecondEmail);
	// Home section
	visible = cvSetNode(data.cvHomeAddress, card.HomeAddress);
	visible = cvSetNode(data.cvHomeAddress2, card.HomeAddress2) || visible;
	visible = cvSetCityStateZip(data.cvHomeCityStZip, card.HomeCity, card.HomeState, card.HomeZipCode) || visible;
	visible = cvSetNode(data.cvHomeCountry, card.HomeCountry) || visible;
	cvSetVisible(data.cvhHome, visible);
	// Other section
	visible = cvSetNodeWithLabel(data.cvCustom1, zCustom1, card.Custom1);
	visible = cvSetNodeWithLabel(data.cvCustom2, zCustom2, card.Custom2) || visible;
	visible = cvSetNodeWithLabel(data.cvCustom3, zCustom3, card.Custom3) || visible;
	visible = cvSetNodeWithLabel(data.cvCustom4, zCustom4, card.Custom4) || visible;
	visible = cvSetNode(data.cvNotes, card.Notes) || visible;
	cvSetVisible(data.cvhOther, visible);
	// Phone section
	visible = cvSetNodeWithLabel(data.cvPhWork, zWork, card.WorkPhone);
	visible = cvSetNodeWithLabel(data.cvPhHome, zHome, card.HomePhone) || visible;
	visible = cvSetNodeWithLabel(data.cvPhFax, zFax, card.FaxNumber) || visible;
	visible = cvSetNodeWithLabel(data.cvPhCellular, zCellular, card.CellularNumber) || visible;
	visible = cvSetNodeWithLabel(data.cvPhPager, zPager, card.PagerNumber) || visible;
	cvSetVisible(data.cvhPhone, visible);
	// Work section
	visible = cvSetNode(data.cvJobTitle, card.JobTitle);
	visible = cvSetNode(data.cvDepartment, card.Department) || visible;
	visible = cvSetNode(data.cvCompany, card.Company) || visible;
	visible = cvSetNode(data.cvWorkAddress, card.WorkAddress) || visible;
	visible = cvSetNode(data.cvWorkAddress2, card.WorkAddress2) || visible;
	visible = cvSetCityStateZip(data.cvWorkCityStZip, card.WorkCity, card.WorkState, card.WorkZipCode) || visible;
	visible = cvSetNode(data.cvWorkCountry, card.WorkCountry) || visible;
	cvSetVisible(data.cvhWork, visible);
}

function ClearCardViewPane()
{
	// FIX ME - waiting for bug fix...cvSetVisible(data.CardViewBox, false);

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
	cvSetVisible(data.cvHomeCityStZip, false);
	// Other section
	cvSetVisible(data.cvhOther, false);
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
	cvSetVisible(data.cvCompany, false);
	cvSetVisible(data.cvWorkAddress, false);
	cvSetVisible(data.cvWorkCityStZip, false);
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

