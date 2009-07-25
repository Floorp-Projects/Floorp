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

  Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefBranch)
            .setIntPref("network.cookie.cookieBehavior", 1);

  var cs = Components.classes["@mozilla.org/cookiemanager;1"]
                     .getService(Components.interfaces.nsICookieManager2);
  cs.removeAll();
  cs.add(domain, "", "oh", "hai", false, false, true, Math.pow(2, 62));
  is(cs.countCookiesFromHost(domain), 1, "cookie inited");

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

    var cookie = theSubject.QueryInterface(this.window.Components.interfaces
                                               .nsIHttpChannel)
                           .getRequestHeader("Cookie");
    this.window.isnot(cookie.indexOf("oh=hai"), -1, "cookie sent");
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

window.addEventListener("message", messageReceiver, false);

/** Receives MessageEvents to this window. */
function messageReceiver(evt)
{
  ok(evt instanceof MessageEvent, "event type", evt);

  if (evt.data != "message") {
    window.removeEventListener("message", messageReceiver, false);

    ok(false, "message", evt.data);

    o.remove();
    gPopup.close();
    SimpleTest.finish();
    return;
  }

  // only run the test when all our children are done loading & setting cookies
  if (++gLoads == gExpectedLoads) {
    window.removeEventListener("message", messageReceiver, false);

    runTest();
  }
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
  is(count, gExpectedCookies, "number of cookies");
  cs.removeAll();

  is(gHeaders, gExpectedHeaders, "number of request headers");

  o.remove();
  gPopup.close();
  SimpleTest.finish();
}
