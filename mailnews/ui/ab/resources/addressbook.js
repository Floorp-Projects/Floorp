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
