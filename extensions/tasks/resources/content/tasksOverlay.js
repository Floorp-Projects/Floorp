function getAppFile(aPrefName)
{
  try 
  {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"];
    prefs = prefs.getService(Components.interfaces.nsIPrefBranch); 
    var appFile = prefs.getComplexValue(aPrefName, Components.interfaces.nsILocalFile); 
    return appFile
  }
  catch (ex) 
  {
    return null;
  }
}

function openApp(aPrefName) 
{
  var appFile = getAppFile(aPrefName);
  if (appFile) 
  {
    try {
      // this should cause the operating system to simulate double clicking 
      // on the location which should launch your calendar application.
      appFile.launch();
    }
    catch (ex)
    { 
    }
  }
}

function haveInternalCalendar()
{
  return ("@mozilla.org/ical-container;1" in Components.classes);
}

function openOtherCal()
{ 
  if (!haveInternalCalendar())
    openApp("task.calendar.location");
}

function OtherTasksOnLoad()
{
  var otherCalTaskBarIcon = document.getElementById("mini-other-cal");
  var otherCalMenuItem = document.getElementById("tasksMenuOtherCal");

  var appFile = getAppFile("task.calendar.location");
  if (appFile && !haveInternalCalendar())
  {
    if (otherCalTaskBarIcon)
      otherCalTaskBarIcon.hidden = false;
    if (otherCalMenuItem)
      otherCalMenuItem.hidden = false;
  }
}

addEventListener("load", OtherTasksOnLoad, false);
