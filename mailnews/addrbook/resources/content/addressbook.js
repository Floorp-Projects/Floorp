function GetDirectoryTree()
{
	var directoryTree = frames[0].frames[0].document.getElementById('dirTree'); 
	return directoryTree;
}

function GetResultTree()
{
	var cardTree = frames[0].frames[1].document.getElementById('resultTree');
	return cardTree;
}

function GetResultTreeDirectory()
{
  var tree = GetResultTree();
  return tree.childNodes[5];
}

function AbNewCard()
{
	var dialog = window.openDialog("chrome://addressbook/content/newcardDialog.xul",
								   "abNewCard",
								   "chrome");
	return dialog;
}


function AbEditCard(card)
{
	var dialog = window.openDialog("chrome://addressbook/content/editcardDialog.xul",
								   "abEditCard",
								   "chrome",
								   {card:card});
	
	return dialog;
}

function AbDelete()
{
	var addressbook = Components.classes["component://netscape/addressbook"].createInstance();
	addressbook = addressbook.QueryInterface(Components.interfaces.nsIAddressBook);
	dump("\AbDelete from XUL\n");
	var tree = GetResultTree();
	if(tree) {
		dump("tree is valid\n");
		//get the selected elements
		var cardList = tree.getElementsByAttribute("selected", "true");
		//get the current folder
		var srcDirectory = GetResultTreeDirectory();
		dump("srcDirectory = " + srcDirectory + "\n");
		addressbook.DeleteCards(tree, srcDirectory, cardList);
	}
}
