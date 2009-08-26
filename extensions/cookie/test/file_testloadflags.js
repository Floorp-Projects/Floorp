var gExpectedCookies;
var gExpectedHeaders;
var gExpectedLoads;

var gObs;
var gPopup;

var gHeaders = 0;
var gLoads = 0;

// setupTest() is run from 'onload='.
function setupTest(uri, domain, cookies, loads, headers) {
  SimpleTest.waitForExplicitFinish();

  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefBranch)
            .setIntPref("network.cookie.cookieBehavior", 1);

  var cs = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager2);
  cs.removeAll();
  cs.add(domain, "", "oh", "hai", false, false, true, Math.pow(2, 62));
  is(cs.countCookiesFromHost(domain), 1, "number of cookies for domain " + domain);

  gExpectedCookies = cookies;
  gExpectedLoads = loads;
  gExpectedHeaders = headers;

  gObs = new obs();
  // Listen for MessageEvents.
  window.addEventListener("message", messageReceiver, false);

  // load a window which contains an iframe; each will attempt to set
  // cookies from their respective domains.
  gPopup = window.open(uri, 'hai', 'width=100,height=100');
}

// Count headers.
function obs () {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  this.window = window;
  this.os = Components.classes["@mozilla.org/observer-service;1"]
                      .getService(Components.interfaces.nsIObserverService);
  this.os.addObserver(this, "http-on-modify-request", false);
}

obs.prototype = {
  observe: function obs_observe (theSubject, theTopic, theData)
  {
    this.window.netscape.security
        .PrivilegeManager.enablePrivilege("UniversalXPConnect");

    var channel = theSubject.QueryInterface(
                    this.window.Components.interfaces.nsIHttpChannel);
    this.window.isnot(channel.getRequestHeader("Cookie").indexOf("oh=hai"), -1,
                      "cookie 'oh=hai' is in header for " + channel.URI.spec);
    ++gHeaders;
  },

  remove: function obs_remove()
  {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    this.os.removeObserver(this, "http-on-modify-request");
    this.os = null;
    this.window = null;
  }
}

/** Receives MessageEvents to this window. */
// Count and check loads.
function messageReceiver(evt)
{
  is(evt.data, "f_lf_i msg data", "message data received from popup");
  if (evt.data != "f_lf_i msg data") {
    gPopup.close();
    window.removeEventListener("message", messageReceiver, false);

    gObs.remove();
    SimpleTest.finish();
    return;
  }

  // only run the test when all our children are done loading & setting cookies
  if (++gLoads == gExpectedLoads) {
    gPopup.close();
    window.removeEventListener("message", messageReceiver, false);

    runTest();
  }
}

// runTest() is run by messageReceiver().
// Check headers, and count and check cookies.
function runTest() {
  // set a cookie from a domain of "localhost"
  document.cookie = "o=noes";

  gObs.remove();

  is(gHeaders, gExpectedHeaders, "number of observed request headers");

  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var cs = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager);
  var count = 0;
  for(var list = cs.enumerator; list.hasMoreElements(); list.getNext())
    ++count;
  is(count, gExpectedCookies, "total number of cookies");
  cs.removeAll();

  SimpleTest.finish();
}
