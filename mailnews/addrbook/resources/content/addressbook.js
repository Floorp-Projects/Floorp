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
	cvData.cvOrganization	= doc.getElementById("cvOrganization");
	cvData.cvWorkAddress	= doc.getElementById("cvWorkAddress");
	cvData.cvWorkCityStZip	= doc.getElementById("cvWorkCityStZip");
}
	
function ChangeDirectoryByDOMNode(dirNode)
{
	var uri = dirNode.getAttribute('id');
	dump(uri + "\n");
	ChangeDirectoryByURI(uri);
}

function ChangeDirectoryByURI(uri)
{
	var tree = frames[0].frames[1].document.getElementById('resultTree');
	//dump("tree = " + tree + "\n");

	var treechildrenList = tree.getElementsByTagName('treechildren');
	if ( treechildrenList.length == 1 )
	{
		var body = treechildrenList[0];
		body.setAttribute('id', uri);// body no longer valid after setting id.
	}
}


function saChangeDirectoryByDOMNode(dirNode)
{
	var uri = dirNode.getAttribute('id');
	dump(uri + "\n");
	saChangeDirectoryByURI(uri);
}

function saChangeDirectoryByURI(uri)
{
	var tree = frames["browser.selAddrResultPane"].document.getElementById('resultTree');
	//dump("tree = " + tree + "\n");

	var treechildrenList = tree.getElementsByTagName('treechildren');
	if ( treechildrenList.length == 1 )
	{
		var body = treechildrenList[0];
		body.setAttribute('id', uri);// body no longer valid after setting id.
	}
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

	var card = Components.classes["component://netscape/rdf/resource-factory?name=abcard"].createInstance();
	card = card.QueryInterface(Components.interfaces.nsIAbCard);
	dump("card = " + card + "\n");

	if (card)
	{
		card.SetCardValue('firstname', document.getElementById('firstname').value);
		card.SetCardValue('lastname', document.getElementById('lastname').value);
		card.SetCardValue('displayname', document.getElementById('displayname').value);
		card.SetCardValue('nickname', document.getElementById('nickname').value);
		card.SetCardValue('primaryemail', document.getElementById('primaryemail').value);
		card.SetCardValue('secondemail', document.getElementById('secondemail').value);
		card.SetCardValue('workphone', document.getElementById('workphone').value);
		card.SetCardValue('homephone', document.getElementById('homephone').value);
		card.SetCardValue('faxnumber', document.getElementById('faxnumber').value);
		card.SetCardValue('pagernumber', document.getElementById('pagernumber').value);
		card.SetCardValue('cellularnumber', document.getElementById('cellularnumber').value);

		card.AddCardToDatabase();
	}
	window.close();
}


function EditCardCancelButton()
{
	dump("Cancel Hit\n");
	window.close();
}

function ResultsPaneSelectionChange()
{
	// not in ab window if no parent.parent.rdf
	if ( parent.parent.rdf )
	{
		var doc = parent.parent.frames["resultsFrame"].document;
		
		var selArray = doc.getElementsByAttribute('selected', 'true');
		if ( selArray && (selArray.length == 1) )
			DisplayCardViewPane(selArray[0]);
		else
			ClearCardViewPane();
	}
}

function DisplayCardViewPane(abNode)
{
	var uri = abNode.getAttribute('id');
	var cardResource = parent.parent.rdf.GetResource(uri);
	var card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
	
	var name = card.personName;// FIX ME - this should be displayName
	
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
	cvSetNode(data.cvNickname, "\"" + card.nickName + "\"");
	cvSetNode(data.cvEmail1, card.primaryEmail);
	cvSetNode(data.cvEmail2, card.secondEmail);
	/* Home section */
	visible = cvSetNode(data.cvHomeAddress, "not yet supported");
	visible = cvSetNode(data.cvHomeCityStZip, "not yet supported") || visible;
	cvSetVisible(data.cvhHome, visible);
	/* Other section */
	visible = cvSetNode(data.cvNotes, "not yet supported");
	cvSetVisible(data.cvhOther, visible);
	/* Phone section */
	visible = cvSetPhone(data.cvPhWork, "Work: ", card.workPhone);
	visible = cvSetPhone(data.cvPhHome, "Home: ", card.homePhone) || visible;
	visible = cvSetPhone(data.cvPhFax, "Fax: ", card.faxNumber) || visible;
	visible = cvSetPhone(data.cvPhCellular, "Cellular: ", card.cellularNumber) || visible;
	visible = cvSetPhone(data.cvPhPager, "Pager: ", card.pagerNumber) || visible;
	cvSetVisible(data.cvhPhone, visible);
	/* Work section */
	visible = cvSetNode(data.cvJobTitle, "not yet supported");
	visible = cvSetNode(data.cvOrganization, card.organization) || visible;
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
	cvSetVisible(data.cvOrganization, false);
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

// -------
// Select Address Window
// -------

function SelectAddressToButton()
{
	AddSelectedAddressesIntoBucket("To: ");
}

function SelectAddressCcButton()
{
	AddSelectedAddressesIntoBucket("Cc: ");
}

function SelectAddressBccButton()
{
	AddSelectedAddressesIntoBucket("Bcc: ");
}

function AddSelectedAddressesIntoBucket(prefix)
{
	var item, uri, rdf, cardResource, card, address;
	var resultsDoc = frames["browser.selAddrResultPane"].document;
	var bucketDoc = frames["browser.addressbucket"].document;
	
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var selArray = resultsDoc.getElementsByAttribute('selected', 'true');
	if ( selArray && selArray.length )
	{
		for ( item = 0; item < selArray.length; item++ )
		{
			uri = selArray[item].getAttribute('id');
			cardResource = rdf.GetResource(uri);
			card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
			address = prefix + "\"" + card.personName + "\" <" + card.email + ">";
			AddAddressIntoBucket(bucketDoc, address);
		}
	}	
	DumpDOM(bucketDoc.documentElement);
}

function AddAddressIntoBucket(doc, address)
{
	var tree = doc.getElementById('addressBucket');

	var item = doc.getElementById("bucketItem");
	
	//newitem.setAttribute("rowID", num);
	//newitem.setAttribute("rowName", name);

	var cell = doc.createElement('treecell');
	var row = doc.createElement('treerow');
	var text = doc.createTextNode(address);
	
	cell.appendChild(text);
	row.appendChild(cell);
	item.appendChild(row);
}

function RemoveSelectedFromBucket()
{
	var item;
	var bucketDoc = frames["browser.addressbucket"].document;
	var body = bucketDoc.getElementById("bucketBody");
	
	var selArray = bucketDoc.getElementsByAttribute('selected', 'true');
	if ( selArray && selArray.length )
	{
		for ( item = selArray.length - 1; item >= 0; item-- )
		{
			body.removeChild(selArray[item]);
		}
	}	
}
