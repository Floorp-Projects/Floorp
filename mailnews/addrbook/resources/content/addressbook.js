function ChangeDirectoryByDOMNode(dirNode)
{
  var uri = dirNode.getAttribute('id');
  dump(uri + "\n");
  ChangeDirectoryByURI(uri);
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

function DisplayCardViewPane(messageNode)
{
	/* Setup vars - this should move to a global var */
	/* data for address book, prefixes: "cvb" = card view box
										"cvh" = crad view header
										"cv"  = card view (normal fields) */

	var doc = parent.frames["cardViewFrame"].document;

	dump("label: " + parent.entities.search.label + "\n");

	/* Card View - Title */
	var CardTitle		= doc.getElementById("CardTitle");
	/* Name section */
	var cvbName			= doc.getElementById("cvbName");
	var cvhName			= doc.getElementById("cvhName");
	var cvNickname		= doc.getElementById("cvNickname");
	var cvEmail1		= doc.getElementById("cvEmail1");
	var cvEmail2		= doc.getElementById("cvEmail2");
	/* Home section */
	var cvbHome			= doc.getElementById("cvbHome");
	var cvHomeAddress	= doc.getElementById("cvHomeAddress");
	var cvHomeCityStZip	= doc.getElementById("cvHomeCityStZip");
	/* Other section */
	var cvbOther		= doc.getElementById("cvbOther");
	var cvNotes			= doc.getElementById("cvNotes");
	/* Phone section */
	var cvbPhone		= doc.getElementById("cvbPhone");
	var cvPhWork		= doc.getElementById("cvPhWork");
	var cvPhHome		= doc.getElementById("cvPhHome");
	var cvPhFax			= doc.getElementById("cvPhFax");
	var cvPhCellular	= doc.getElementById("cvPhCellular");
	var cvPhPager		= doc.getElementById("cvPhPager");
	/* Work section */
	var cvbWork			= doc.getElementById("cvbWork");
	var cvJobTitle		= doc.getElementById("cvJobTitle");
	var cvOrganization	= doc.getElementById("cvOrganization");
	var cvWorkAddress	= doc.getElementById("cvWorkAddress");
	var cvWorkCityStZip	= doc.getElementById("cvWorkCityStZip");

	/* set fields in card view pane */
	CardTitle.childNodes[0].nodeValue		= "Card for Winnie the Pooh";
	
	/* Name section */
	cvhName.childNodes[0].nodeValue			= "Winnie the Pooh";
	cvNickname.childNodes[0].nodeValue		= "\"Pooh\"";
	cvEmail1.childNodes[0].nodeValue		= "wpooh@netscape.com";
	cvEmail2.childNodes[0].nodeValue		= "poohbear@netscape.net";
	/* Home section */
	cvHomeAddress.childNodes[0].nodeValue	= "123 Treehouse Lane";
	cvHomeCityStZip.childNodes[0].nodeValue	= "Hundred Acre Wood, CA 94087";
	/* Other section */
	cvNotes.childNodes[0].nodeValue			= "This data is fake.  It is inserted into the DOM from JavaScript.";
	/* Phone section */
	cvPhWork.childNodes[0].nodeValue		= "Work: (650) 937-1234";
	cvPhHome.childNodes[0].nodeValue		= "Home: (408) 732-1212";
	cvPhFax.childNodes[0].nodeValue			= "Fax: (650) 937-3434";
	cvPhCellular.childNodes[0].nodeValue	= "Cellular: (408) 734-9090";
	cvPhPager.childNodes[0].nodeValue		= "Pager: (408) 732-6545";
	/* Work section */
	cvJobTitle.childNodes[0].nodeValue		= "Interaction Designer";
	cvOrganization.childNodes[0].nodeValue	= "Netscape Communications Corp.";
	cvWorkAddress.childNodes[0].nodeValue	= "501 E Middlefield Road";
	cvWorkCityStZip.childNodes[0].nodeValue	= "Mountain View, CA 94043";
}
