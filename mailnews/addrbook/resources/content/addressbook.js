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
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore)
	{
		toolkitCore = new ToolkitCore();
		if (toolkitCore)
		{
			toolkitCore.Init("ToolkitCore");
		}
    }

    if (toolkitCore)
	{
      toolkitCore.ShowWindow("chrome://addressbook/content/editcard.xul", window);
    }
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
