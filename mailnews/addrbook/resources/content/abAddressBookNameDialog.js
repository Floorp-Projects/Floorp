var okCallback = 0;

function abNameOnLoad()
{
	doSetOKCancel(abNameOKButton, 0);

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
	
	// focus on input
	var name = document.getElementById('name');
	if ( name )
		name.focus();
	moveToAlertPosition();
}

function abNameOKButton()
{
	if ( top.okCallback )
	{
		var name = document.getElementById('name').value;
	
		top.okCallback(name);
	}
	
	return true;
}
