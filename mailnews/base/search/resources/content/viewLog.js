var gFilterList;
var gLogFilters;
var gLogView;

function onLoad()
{
  gFilterList = window.arguments[0].filterList;

  gLogFilters = document.getElementById("logFilters");
  gLogFilters.checked = gFilterList.loggingEnabled;

  gLogView = document.getElementById("logView");

  // for security, disable JS
  gLogView.docShell.allowJavascript = false;

  // if log doesn't exist, this will create an empty file on disk
  // otherwise, the user will get an error that the file doesn't exist
  gFilterList.ensureLogFile();

  gLogView.setAttribute("src", gFilterList.logURL);
}

function toggleLogFilters()
{
  gFilterList.loggingEnabled =  gLogFilters.checked;
}

function clearLog()
{
  gFilterList.clearLog();

  // reload the newly truncated file
  gLogView.reload();
}

