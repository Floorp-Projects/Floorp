var gFilterList;
var gLogFilters;

function onLoad()
{
  gFilterList = window.arguments[0].filterList;
  gLogFilters = document.getElementById("logFilters");
  gLogFilters.checked = gFilterList.loggingEnabled;

  var logView = document.getElementById("logView");
  // won't work yet
  // Security Error: Content at chrome://messenger/content/filterLog.xul 
  // may not load or link to file:///C|/Documents%20and%20Settings/Administrator/Application%20Data/Mozilla/Profiles/sspitzer/js48p4bs.slt/ImapMail/nsmail-1/filterlog.html.
  logView.setAttribute("src", gFilterList.logURL);
}

function toggleLogFilters()
{
   gFilterList.loggingEnabled = gLogFilters.checked;
}

function clearLog()
{
}
