var okCallback = 0;

function abNameOnLoad()
{
	doSetOKCancel(abNameOKButton, 0);

	// look in arguments[0] for parameters
	if ("arguments" in window && window.arguments[0])
	{
		if ("title" in window.arguments[0])
		{
			var title = window.arguments[0].title;
			top.window.title = title;
		}
		
		if ("okCallback" in window.arguments[0])
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
