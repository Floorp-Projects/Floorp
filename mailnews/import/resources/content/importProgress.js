
var progressMeter = 0;
var progressInfo = null;

function OnLoadProgressDialog()
{
	
	
	dump( "*** Loaded progress window\n");

	top.progressMeter = document.getElementById( 'progressMeter');

	// look in arguments[0] for parameters
	if (window.arguments && window.arguments[0])
	{
		// keep parameters in global for later
		if ( window.arguments[0].windowTitle )
			top.window.title = window.arguments[0].windowTitle;
		
		if ( window.arguments[0].progressTitle )
			SetDivText( 'ProgressTitle', window.arguments[0].progressTitle );
		
		if ( window.arguments[0].progressStatus )
			SetDivText( 'ProgressStatus', window.arguments[0].progressStatus );
		
		top.progressInfo = window.arguments[0].progressInfo;
	}

	top.progressInfo.progressWindow = top.window;
	top.progressInfo.intervalState = setInterval( "Continue()", 500);

}

function SetProgress( val)
{
	top.progressMeter.setAttribute( "value", val);
}

function SetDivText(id, text)
{
	var div = document.getElementById(id);
	
	if ( div )
	{
		if ( div.childNodes.length == 0 )
		{
			var textNode = document.createTextNode(text);
			div.appendChild(textNode);                   			
		}
		else if ( div.childNodes.length == 1 )
			div.childNodes[0].nodeValue = text;
	}
}

function Continue()
{
	top.progressInfo.mainWindow.ContinueImport( top.progressInfo);
}

