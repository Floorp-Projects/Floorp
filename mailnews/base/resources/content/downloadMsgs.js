var gPrefs;

function doYesButton()
{
	var alwaysAskForDownload = document.getElementById("offline.downloadMessages").checked;
    if (!gPrefs) GetPrefsService();

	if(alwaysAskForDownload) gPrefs.SetIntPref("offline.download.download_messages", 0);

	window.arguments[0].result = 1;
	window.close();
}

function doNoButton()
{
	var alwaysAskForDownload = document.getElementById("offline.downloadMessages").checked;
    if (!gPrefs) GetPrefsService();

	if(alwaysAskForDownload) gPrefs.SetIntPref("offline.download.download_messages", 0);

	window.arguments[0].result = 2;
	window.close();
}

function doCancelButton()
{
	window.arguments[0].result = 3;
	window.close();
}

function GetPrefsService()
{
   // Store the prefs object
   try {
		var prefsService = Components.classes["@mozilla.org/preferences;1"];
		if (prefsService)
		prefsService = prefsService.getService();
		if (prefsService)
		gPrefs = prefsService.QueryInterface(Components.interfaces.nsIPref);

		if (!gPrefs)
		dump("failed to get prefs service!\n");
   }
   catch(ex) {
		 dump("failed to get prefs service!\n");
   }
}

