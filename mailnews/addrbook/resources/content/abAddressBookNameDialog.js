var okCallback = 0;
var gCanRename = true;
var gOkButton;
var gNameInput;

function abNameOnLoad()
{
  var abName = "";
  
  gOkButton = document.documentElement.getButton('accept');

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
  gNameInput = document.getElementById('name');
  if (gNameInput) {
    if (abName)
      gNameInput.value = abName;
    
    if (gCanRename)
      gNameInput.focus();
    else
      gNameInput.disabled = true;
  }

  abNameDoOkEnabling()

	moveToAlertPosition();
}

function abNameOKButton()
{
	if (top.okCallback && gCanRename)
    top.okCallback(gNameInput.value.replace(/^\s+|\s+$/g, ''));
	
	return true;
}

function abNameDoOkEnabling()
{
  gOkButton.disabled = !/\S/.test(gNameInput.value);
}
