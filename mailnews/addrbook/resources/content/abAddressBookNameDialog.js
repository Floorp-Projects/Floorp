var okCallback = 0;
var gCanRename = true;

function abNameOnLoad()
{
  var abName = "";
  
	// look in arguments[0] for parameters
	if ("arguments" in window && window.arguments[0])
	{
    if ("title" in window.arguments[0])
      document.title = window.arguments[0].title;

		if ("okCallback" in window.arguments[0])
			top.okCallback = window.arguments[0].okCallback;

    if ("name" in window.arguments[0])
      abName = window.arguments[0].name;

    if ("canRename" in window.arguments[0])
      gCanRename = window.arguments[0].canRename;
	}
	
	// focus on input
	var name = document.getElementById('name');
  if (name) {
    if (abName)
      name.value = abName;
    
    if (gCanRename)
      name.focus();
    else
      name.disabled = true;
  }

	moveToAlertPosition();
}

function abNameOKButton()
{
	if (top.okCallback && gCanRename)
    top.okCallback(document.getElementById('name').value);
	
	return true;
}
