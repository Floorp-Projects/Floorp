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
