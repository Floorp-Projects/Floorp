var gLogView;
var gLogJunk;
var gSpamSettings;

function onLoad()
{
  gSpamSettings = window.arguments[0].spamSettings;

  gLogJunk = document.getElementById("logJunk");
  gLogJunk.checked = gSpamSettings.loggingEnabled;

  gLogView = document.getElementById("logView");

  // for security, disable JS
  gLogView.docShell.allowJavascript = false;

  // if log doesn't exist, this will create an empty file on disk
  // otherwise, the user will get an error that the file doesn't exist
  gSpamSettings.ensureLogFile();

  gLogView.setAttribute("src", gSpamSettings.logURL);
}

function toggleJunkLog()
{
  gSpamSettings.loggingEnabled =  gLogJunk.checked;
}

function clearLog()
{
  gSpamSettings.clearLog();

  // reload the newly truncated file
  gLogView.reload();
}

