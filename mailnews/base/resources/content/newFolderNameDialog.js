var okCallback = 0;

function newFolderNameOnLoad()
{
	doSetOKCancel(newFolderNameOKButton, 0);

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0])
	{
		if ( window.arguments[0].title )
		{
			dump("title = " + window.arguments[0].title + "\n");
			var title = window.arguments[0].title;
			top.window.title = title;
		}
		
		if ( window.arguments[0].okCallback )
			top.okCallback = window.arguments[0].okCallback;
	}
}

function newFolderNameOKButton()
{
	if ( top.okCallback )
	{
		var name = document.getElementById('name').value;
	
		top.okCallback(name);
	}
	
	return true;
}
