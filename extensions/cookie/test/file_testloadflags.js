SimpleTest.waitForExplicitFinish();

var gPopup = null;

var gExpectedCookies = 0;
var gExpectedLoads = 0;
var gExpectedHeaders = 0;
var gLoads = 0;
var gHeaders = 0;

var o = null;

function setupTest(uri, domain, cookies, loads, headers) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  prefs.setIntPref("network.cookie.cookieBehavior", 1);

  var cs = Components.classes["@mozilla.org/cookiemanager;1"]
                      .getService(Components.interfaces.nsICookieManager2);
  cs.removeAll();
  cs.add(domain, "", "oh", "hai", false, false, true, Math.pow(2, 62));
  is(cs.countCookiesFromHost(domain), 1, "cookie wasn't inited");

  o = new obs();

  gExpectedCookies = cookies;
  gExpectedLoads = loads;
  gExpectedHeaders = headers;

  // load a window which contains an iframe; each will attempt to set
  // cookies from their respective domains.
  gPopup = window.open(uri, 'hai', 'width=100,height=100');
}

function obs () {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  this.os = Components.classes["@mozilla.org/observer-service;1"]
                      .getService(Components.interfaces.nsIObserverService);
  this.os.addObserver(this, "http-on-modify-request", false);
  this.window = window;
}

obs.prototype = {
  observe: function obs_observe (theSubject, theTopic, theData)
  {
    this.window.netscape.security
        .PrivilegeManager.enablePrivilege("UniversalXPConnect");
    var httpchannel = theSubject.QueryInterface(this.window.Components.interfaces
                                                    .nsIHttpChannel);

    var cookie = httpchannel.getRequestHeader("Cookie");

    var got = cookie.indexOf("oh=hai");
    this.window.isnot(got, -1, "cookie wasn't sent");
    gHeaders++;
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
function messageReceiver(evt)
{
  ok(evt instanceof MessageEvent, "wrong event type");

  if (evt.data == "message")
    gLoads++;
  else {
    ok(false, "wrong message");
    o.remove();
    gPopup.close();
    SimpleTest.finish();
  }

  // only run the test when all our children are done loading & setting cookies
  if (gLoads == gExpectedLoads)
    runTest();
}

function runTest() {
  // set a cookie from a domain of "localhost"
  document.cookie = "o=noes";

  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var cs = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager);
  var list = cs.enumerator;
  var count = 0;
  while (list.hasMoreElements()) {
    count++;
    list.getNext();
  }
  is(count, gExpectedCookies, "incorrect number of cookies");
  is(gHeaders, gExpectedHeaders, "incorrect number of request headers");

  o.remove();
  gPopup.close();
  cs.removeAll();
  SimpleTest.finish();
}

document.addEventListener("message", messageReceiver, false);

