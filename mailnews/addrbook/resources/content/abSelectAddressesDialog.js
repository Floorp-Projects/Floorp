var composeWindow = 0;
var msgCompFields = 0;

// localization strings
var prefixTo = "To: ";
var prefixCc = "Cc: ";
var prefixBcc = "Bcc: ";

function OnLoadSelectAddress()
{
	var toAddress="", ccAddress="", bccAddress="";

	doSetOKCancel(SelectAddressOKButton, 0);

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0])
	{
		// keep parameters in global for later
		if ( window.arguments[0].composeWindow )
			top.composeWindow = window.arguments[0].composeWindow;
		if ( window.arguments[0].msgCompFields )
			top.msgCompFields = window.arguments[0].msgCompFields;
		if ( window.arguments[0].toAddress )
			toAddress = window.arguments[0].toAddress;
		if ( window.arguments[0].ccAddress )
			ccAddress = window.arguments[0].ccAddress;
		if ( window.arguments[0].bccAddress )
			bccAddress = window.arguments[0].bccAddress;
			
		dump("onload top.composeWindow: " + top.composeWindow + "\n");
		dump("onload toAddress: " + toAddress + "\n");

		// put the addresses into the bucket
		AddAddressFromComposeWindow(toAddress, prefixTo);
		AddAddressFromComposeWindow(ccAddress, prefixCc);
		AddAddressFromComposeWindow(bccAddress, prefixBcc);
	}
}

function AddAddressFromComposeWindow(addresses, prefix)
{
	if ( addresses )
	{
		var addressArray = addresses.split(",");
		
		for ( var index = 0; index < addressArray.length; index++ )
		{
			// remove leading spaces
			while ( addressArray[index][0] == " " )
				addressArray[index] = addressArray[index].substring(1, addressArray[index].length);
			
			AddAddressIntoBucket(prefix + addressArray[index]);
		}
	}
}


function SelectAddressOKButton()
{
	var body = document.getElementById('bucketBody');
	var item, row, cell, text, colon;
	var toAddress="", ccAddress="", bccAddress="";
	
	for ( var index = 0; index < body.childNodes.length; index++ )
	{
		item = body.childNodes[index];
		if ( item.childNodes && item.childNodes.length )
		{
			row = item.childNodes[0];
			if (  row.childNodes &&  row.childNodes.length )
			{
				cell = row.childNodes[0];
				if ( cell.childNodes &&  cell.childNodes.length )
				{
					text = cell.childNodes[0];
					if ( text && text.data && text.data.length )
					{
						switch ( text.data[0] )
						{
							case prefixTo[0]:
								if ( toAddress )
									toAddress += ", ";
								toAddress += text.data.substring(prefixTo.length, text.data.length);
								break;
							case prefixCc[0]:
								if ( ccAddress )
									ccAddress += ", ";
								ccAddress += text.data.substring(prefixCc.length, text.data.length);
								break;
							case prefixBcc[0]:
								if ( bccAddress )
									bccAddress += ", ";
								bccAddress += text.data.substring(prefixBcc.length, text.data.length);
								break;
						}
					}
				}
			}
		}
	}
	
	// reset the UI in compose window
	msgCompFields.SetTo(toAddress);
	msgCompFields.SetCc(ccAddress);
	msgCompFields.SetBcc(bccAddress);
	top.composeWindow.CompFields2Recipients(top.msgCompFields);

	return true;
}

function SelectAddressToButton()
{
	AddSelectedAddressesIntoBucket(prefixTo);
}

function SelectAddressCcButton()
{
	AddSelectedAddressesIntoBucket(prefixCc);
}

function SelectAddressBccButton()
{
	AddSelectedAddressesIntoBucket(prefixBcc);
}

function SelectAddressNewButton()
{
	AbNewCardDialog();
}

function AddSelectedAddressesIntoBucket(prefix)
{
	var item, uri, rdf, cardResource, card, address;
	
	rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var resultsTree = document.getElementById('resultsTree');
	
	if ( resultsTree && resultsTree.selectedItems && resultsTree.selectedItems.length )
	{
		for ( item = 0; item < resultsTree.selectedItems.length; item++ )
		{
			uri = resultsTree.selectedItems[item].getAttribute('id');
			cardResource = rdf.GetResource(uri);
			card = cardResource.QueryInterface(Components.interfaces.nsIAbCard);
			address = prefix + "\"" + card.DisplayName + "\" <" + card.PrimaryEmail + ">";
			AddAddressIntoBucket(address);
		}
	}	
}

function AddAddressIntoBucket(address)
{
	var body = document.getElementById("bucketBody");
	
	var item = document.createElement('treeitem');
	var row = document.createElement('treerow');
	var cell = document.createElement('treecell');
	var text = document.createTextNode(address);
	
	cell.appendChild(text);
	row.appendChild(cell);
	item.appendChild(row);
	body.appendChild(item);
}

function RemoveSelectedFromBucket()
{
	var bucketTree = document.getElementById("addressBucket");
	if ( bucketTree )
	{
		var body = document.getElementById("bucketBody");
		
		if ( body && bucketTree.selectedItems && bucketTree.selectedItems.length )
		{
			for ( var item = bucketTree.selectedItems.length - 1; item >= 0; item-- )
				body.removeChild(bucketTree.selectedItems[item]);
		}	
	}	
}
