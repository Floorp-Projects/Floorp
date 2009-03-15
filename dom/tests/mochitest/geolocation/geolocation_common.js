
function stop_geolocationProvider()
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(null, "geolocation-test-control", "stop-responding");
}

function resume_geolocationProvider()
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(null, "geolocation-test-control", "start-responding");
}

function check_geolocation(location) {

  ok(location, "Check to see if this location is non-null");

  ok("timestamp" in location, "Check to see if there is a timestamp");

  // eventually, coords may be optional (eg, when civic addresses are supported)
  ok("coords" in location, "Check to see if this location has a coords");

  var coords = location.coords;

  ok("latitude" in coords, "Check to see if there is a latitude");
  ok("longitude" in coords, "Check to see if there is a longitude");
  ok("altitude" in coords, "Check to see if there is a altitude");
  ok("accuracy" in coords, "Check to see if there is a accuracy");
  ok("altitudeAccuracy" in coords, "Check to see if there is a alt accuracy");
  ok("heading" in coords, "Check to see if there is a heading");
  ok("speed" in coords, "Check to see if there is a speed");
}


function getNotificationBox()
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  const Ci = Components.interfaces;
  
  function getChromeWindow(aWindow) {
      var chromeWin = aWindow 
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebNavigation)
          .QueryInterface(Ci.nsIDocShellTreeItem)
          .rootTreeItem
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindow)
          .QueryInterface(Ci.nsIDOMChromeWindow);
      return chromeWin;
  }

  var notifyWindow = window.top;

  var chromeWin = getChromeWindow(notifyWindow);

  var notifyBox = chromeWin.getNotificationBox(notifyWindow);

  return notifyBox;
}


function clickNotificationButton(aBar, aButtonName) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  // This is a bit of a hack. The notification doesn't have an API to
  // trigger buttons, so we dive down into the implementation and twiddle
  // the buttons directly.
  var buttons = aBar.getElementsByTagName("button");
  var clicked = false;
  for (var i = 0; i < buttons.length; i++) {
      if (buttons[i].label == aButtonName) {
          buttons[i].click();
          clicked = true;
          break;
      }
  }
  
  ok(clicked, "Clicked \"" + aButtonName + "\" button"); 
}


function clickAccept()
{
  clickNotificationButton(getNotificationBox().currentNotification, "Exact Location (within 10 feet)");
}

function clickDeny()
{
  clickNotificationButton(getNotificationBox().currentNotification, "Nothing");
}
