var okCallback = 0;

function abNameOnLoad()
{
  var abName = "";
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

    if ("name" in window.arguments[0])
      abName = window.arguments[0].name;
	}
	
	// focus on input
	var name = document.getElementById('name');
  if (name) {
    if (abName)
      name.value = abName;
		name.focus();
  }

	moveToAlertPosition();
}

function abNameOKButton()
{
	if ( top.okCallback )
    top.okCallback(document.getElementById('name').value);
	
	return true;
}
