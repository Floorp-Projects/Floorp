var gLogView;
var gLogJunk;
var gXXX;

function onLoad()
{
  return;

  gXXX = window.arguments[0].xxx;

  gLogJunk = document.getElementById("logJunk");
  gLogJunk.checked = gXXX.loggingEnabled;

  gLogView = document.getElementById("logView");

  // for security, disable JS
  gLogView.docShell.allowJavascript = false;

  // if log doesn't exist, this will create an empty file on disk
  // otherwise, the user will get an error that the file doesn't exist
  gXXX.ensureLogFile();

  gLogView.setAttribute("src", gXXX.logURL);
}

function toggleJunkLog()
{
  gXXX.loggingEnabled =  gLogJunk.checked;
}

function clearLog()
{
  gXXX.clearLog();

  // reload the newly truncated file
  gLogView.setAttribute("src", gXXX.logURL);
}

