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
	cvData.cvOrganization	= doc.getElementById("cvOrganization");
	cvData.cvWorkAddress	= doc.getElementById("cvWorkAddress");
	cvData.cvWorkCityStZip	= doc.getElementById("cvWorkCityStZip");
}
	
function ChangeDirectoryByDOMNode(dirNode)
{
	var uri = dirNode.getAttribute('id');
	dump(uri + "\n");
	ChangeDirectoryByURI(uri);
	
	// HACK to clear the card view pane - when onchange="MyOnChangeFunction()" is working
	// then we can do this correctly
	ClearCardViewPane();
}


function ChangeDirectoryByURI(uri)
{
	var tree = frames[0].frames[1].document.getElementById('resultTree');
	tree.childNodes[7].setAttribute('id', uri);
}


function EditCard() 
{
	var dialog = window.openDialog("chrome://addressbook/content/editcard.xul",
								   "editCard",
								   "chrome");
	return dialog;
}


function SelectAddress() 
{
	var dialog = window.openDialog("chrome://addressbook/content/selectaddress.xul",
								   "selectAddress",
								   "chrome");
	return dialog;
}


function AbNewCard()
{
	EditCard();
}


function EditCardOKButton()
{
	dump("OK Hit\n");
}


function EditCardCancelButton()
{
	dump("Cancel Hit\n");
}

function ResultsPaneSelectionChange(abNode)
{
	var doc = parent.parent.frames["resultsFrame"].document;
	
	var selArray = doc.getElementsByAttribute('selected', 'true');
	if ( selArray && (selArray.length == 1) )
		DisplayCardViewPane(selArray[0]);
	else
		ClearCardViewPane();
}

function DisplayCardViewPane(abNode)
{
	var uri = abNode.getAttribute('id');
	var cardResource = parent.parent.rdf.GetResource(uri);
	var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
	
	var name = card.personName;
	
	var data = parent.parent.cvData;
	var visible;
	
	/* set fields in card view pane */
	visible = cvSetNode(data.CardTitle, "Card for " + name);
	
	// FIX ME!
	// Code needs to be fixed to make the entire box visible or not.  Current hack just hides
	// the header of the section that should be visible.
	
	/* Name section */
	cvSetNode(data.cvhName, name);
	cvSetNode(data.cvNickname, "\"" + card.nickname + "\"");
	cvSetNode(data.cvEmail1, card.email);
	cvSetNode(data.cvEmail2, "poohbear@netscape.net");
	/* Home section */
	visible = cvSetNode(data.cvHomeAddress, "123 Treehouse Lane");
	visible = cvSetNode(data.cvHomeCityStZip, "Hundred Acre Wood, CA 94087") && visible;
	cvSetVisible(data.cvhHome, visible);
	/* Other section */
	visible = cvSetNode(data.cvNotes, "This data is fake.  It is inserted into the DOM from JavaScript.");
	cvSetVisible(data.cvhOther, visible);
	/* Phone section */
	visible = cvSetNode(data.cvPhWork, "Work: " + card.workPhone);
	visible = cvSetNode(data.cvPhHome, "Home: (408) 732-1212") && visible;
	visible = cvSetNode(data.cvPhFax, "Fax: (650) 937-3434") && visible;
	visible = cvSetNode(data.cvPhCellular, "Cellular: (408) 734-9090") && visible;
	visible = cvSetNode(data.cvPhPager, "Pager: (408) 732-6545") && visible;
	cvSetVisible(data.cvhPhone, visible);
	/* Work section */
	visible = cvSetNode(data.cvJobTitle, "Interaction Designer");
	visible = cvSetNode(data.cvOrganization, card.organization) && visible;
	visible = cvSetNode(data.cvWorkAddress, "501 E Middlefield Road") && visible;
	visible = cvSetNode(data.cvWorkCityStZip, card.city + ", CA 94043") && visible;
	cvSetVisible(data.cvhWork, visible);
}

function ClearCardViewPane()
{
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
	cvSetVisible(data.cvOrganization, false);
	cvSetVisible(data.cvWorkAddress, false);
	cvSetVisible(data.cvWorkCityStZip, false);
}

function cvSetNode(node, text)
{
	node.childNodes[0].nodeValue = text;
	if ( text == "" )
	{
		node.style.display = "none";
		return false;
	}
	else
	{
		node.style.display = "block";
		return true;
	}
}

function cvSetVisible(node, visible)
{
	if ( visible )
		node.style.display = "block";
	else
		node.style.display = "none";
}
