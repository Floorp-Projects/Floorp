var rdf;
var cvData;

function OnLoadAddressBook()
{
	// This should be in an onload for the card view window, but that is not currently working
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var doc = frames["cardViewFrame"].document;
	
	/* data for address book, prefixes: "cvb" = card view box
										"cvh" = crad view header
										"cv"  = card view (normal fields) */
	cvData = new Object;

	// Card View Box
	cvData.CardViewBox		= doc.getElementById("CardViewBox");
	// Title
	cvData.CardTitle		= doc.getElementById("CardTitle");
	// Name section
	cvData.cvhName			= doc.getElementById("cvhName");
	cvData.cvNickname		= doc.getElementById("cvNickname");
	cvData.cvEmail1			= doc.getElementById("cvEmail1");
	cvData.cvEmail2			= doc.getElementById("cvEmail2");
	// Home section
	cvData.cvhHome			= doc.getElementById("cvhHome");
	cvData.cvHomeAddress	= doc.getElementById("cvHomeAddress");
	cvData.cvHomeCityStZip	= doc.getElementById("cvHomeCityStZip");
	// Other section
	cvData.cvhOther			= doc.getElementById("cvhOther");
	cvData.cvNotes			= doc.getElementById("cvNotes");
	// Phone section
	cvData.cvhPhone			= doc.getElementById("cvhPhone");
	cvData.cvPhWork			= doc.getElementById("cvPhWork");
	cvData.cvPhHome			= doc.getElementById("cvPhHome");
	cvData.cvPhFax			= doc.getElementById("cvPhFax");
	cvData.cvPhCellular		= doc.getElementById("cvPhCellular");
	cvData.cvPhPager		= doc.getElementById("cvPhPager");
	// Work section
	cvData.cvhWork			= doc.getElementById("cvhWork");
	cvData.cvJobTitle		= doc.getElementById("cvJobTitle");
	cvData.cvCompany		= doc.getElementById("cvCompany");
	cvData.cvWorkAddress	= doc.getElementById("cvWorkAddress");
	cvData.cvWorkCityStZip	= doc.getElementById("cvWorkCityStZip");
}
	
function DisplayCardViewPane(abNode)
{
	var uri = abNode.getAttribute('id');
	var cardResource = parent.parent.rdf.GetResource(uri);
	var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
	
	var name = card.DisplayName;// FIX ME - this should be displayName
	
	var data = parent.parent.cvData;
	var visible;
	
	/* set fields in card view pane */
	// FIX ME - waiting for bug fix... cvSetVisible(data.CardViewBox, true);
	cvSetNode(data.CardTitle, "Card for " + name);
	
	// FIX ME!
	// Code needs to be fixed to make the entire box visible or not.  Current hack just hides
	// the header of the section that should be visible.
	
	/* Name section */
	cvSetNode(data.cvhName, name);
	cvSetNode(data.cvNickname, "\"" + card.NickName + "\"");
	cvSetNode(data.cvEmail1, card.PrimaryEmail);
	cvSetNode(data.cvEmail2, card.SecondEmail);
	/* Home section */
	visible = cvSetNode(data.cvHomeAddress, "not yet supported");
	visible = cvSetNode(data.cvHomeCityStZip, "not yet supported") || visible;
	cvSetVisible(data.cvhHome, visible);
	/* Other section */
	visible = cvSetNode(data.cvNotes, "not yet supported");
	cvSetVisible(data.cvhOther, visible);
	/* Phone section */
	visible = cvSetPhone(data.cvPhWork, "Work: ", card.WorkPhone);
	visible = cvSetPhone(data.cvPhHome, "Home: ", card.HomePhone) || visible;
	visible = cvSetPhone(data.cvPhFax, "Fax: ", card.FaxNumber) || visible;
	visible = cvSetPhone(data.cvPhCellular, "Cellular: ", card.CellularNumber) || visible;
	visible = cvSetPhone(data.cvPhPager, "Pager: ", card.PagerNumber) || visible;
	cvSetVisible(data.cvhPhone, visible);
	/* Work section */
	visible = cvSetNode(data.cvJobTitle, "not yet supported");
	visible = cvSetNode(data.cvCompany, card.Company) || visible;
	visible = cvSetNode(data.cvWorkAddress, "not yet supported") || visible;
	visible = cvSetNode(data.cvWorkCityStZip, "not yet supported") || visible;
	cvSetVisible(data.cvhWork, visible);
}

function ClearCardViewPane()
{
	// FIX ME - waiting for bug fix...cvSetVisible(data.CardViewBox, false);

	// HACK - we need to be able to set the entire box or div to display:none when bug fixed
	var data = parent.parent.cvData;

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

function cvSetPhone(node, phone, text)
{
	if ( text )
		return cvSetNode(node, phone + text);
	else
		return cvSetNode(node, "");
}

function cvSetNode(node, text)
{
	node.childNodes[0].nodeValue = text;
	if ( text )
	{
		node.setAttribute("style", "display:block");
		return true;
	}
	else
	{
		node.setAttribute("style", "display:none");
		return false;
	}
}

function cvSetVisible(node, visible)
{
	if ( visible )
		node.setAttribute("style", "display:block");
	else
		node.setAttribute("style", "display:none");
}

