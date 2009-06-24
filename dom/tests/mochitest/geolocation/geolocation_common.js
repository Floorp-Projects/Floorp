
function stop_geolocationProvider()
{
  var baseURL = "http://localhost:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", baseURL + "?action=stop-responding&latitude=3.14", false);
  xhr.send(null);
}

function resume_geolocationProvider()
{
  var baseURL = "http://localhost:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", baseURL + "?action=start-responding", false);
  xhr.send(null);
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


function clickNotificationButton(aButtonIndex) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  // This is a bit of a hack. The notification doesn't have an API to
  // trigger buttons, so we dive down into the implementation and twiddle
  // the buttons directly.
  var box = getNotificationBox();
  ok(box, "Got notification box");
  var bar = box.getNotificationWithValue("geolocation");
  ok(bar, "Got geolocation notification");
  var button = bar.getElementsByTagName("button").item(aButtonIndex);
  ok(button, "Got button");
  button.doCommand();
}

const kAcceptButton = 0;
const kDenyButton = 1;
