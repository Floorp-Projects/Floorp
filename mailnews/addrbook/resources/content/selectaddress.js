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

function SelectAddressOKButton()
{
	top.window.close();
}

function SelectAddressCancelButton()
{
	top.window.close();
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
	var bucketDoc = frames["browser.addressbucket"].document;
	dump("bucketDoc = " + bucketDoc + "\n");
	var item = bucketDoc.getElementById("bucketItem");
	dump("item = " + item + "\n");
	
	var selArray = item.getElementsByAttribute('selected', 'true');
	dump("selArray = " + selArray + "\n");
	if ( selArray && selArray.length )
	{
		for ( var row = selArray.length - 1; row >= 0; row-- )
		{
			dump("selArray[row] = " + selArray[row] + "\n");
			DumpDOM(selArray[row]);
			item.removeChild(selArray[row]);
		}
	}	
}
