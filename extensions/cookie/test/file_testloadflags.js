var gExpectedCookies;
var gExpectedHeaders;
var gExpectedLoads;

var gObs;
var gPopup;

var gHeaders = 0;
var gLoads = 0;

// setupTest() is run from 'onload='.
function setupTest(uri, domain, cookies, loads, headers) {
  ok(true, "setupTest uri: " + uri + " domain: " + domain + " cookies: " + cookies +
           " loads: " + loads + " headers: " + headers);

  SimpleTest.waitForExplicitFinish();

  SpecialPowers.Cc["@mozilla.org/preferences-service;1"]
               .getService(SpecialPowers.Ci.nsIPrefBranch)
               .setIntPref("network.cookie.cookieBehavior", 1);

  var cs = SpecialPowers.Cc["@mozilla.org/cookiemanager;1"]
                        .getService(SpecialPowers.Ci.nsICookieManager2);

  ok(true, "we are going to remove these cookies");
  var count = 0;
  var list = cs.enumerator;
  while (list.hasMoreElements()) {
    var cookie = list.getNext().QueryInterface(SpecialPowers.Ci.nsICookie);
    ok(true, "cookie: " + cookie);
    ok(true, "cookie host " + cookie.host + " path " + cookie.path + " name " + cookie.name +
       " value " + cookie.value + " isSecure " + cookie.isSecure + " expires " + cookie.expires);
    ++count;
  }
  ok(true, count + " cookies");

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

function finishTest()
{
  gObs.remove();

  SpecialPowers.Cc["@mozilla.org/preferences-service;1"]
               .getService(SpecialPowers.Ci.nsIPrefBranch)
               .clearUserPref("network.cookie.cookieBehavior");

  SimpleTest.finish();
}

// Count headers.
function obs () {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  ok(true, "adding observer");

  this.window = window;
  this.os = SpecialPowers.Cc["@mozilla.org/observer-service;1"]
                         .getService(SpecialPowers.Ci.nsIObserverService);
  this.os.addObserver(this, "http-on-modify-request", false);
}

obs.prototype = {
  observe: function obs_observe (theSubject, theTopic, theData)
  {
    ok(true, "theSubject " + theSubject);
    ok(true, "theTopic " + theTopic);
    ok(true, "theData " + theData);

    var channel = theSubject.QueryInterface(
                    this.window.SpecialPowers.Ci.nsIHttpChannel);
    ok(true, "channel " + channel);
    try {
      ok(true, "channel.URI " + channel.URI);
      ok(true, "channel.URI.spec " + channel.URI.spec);
      channel.visitRequestHeaders({
        visitHeader: function(aHeader, aValue) {
          ok(true, aHeader + ": " + aValue);
        }});
    } catch (err) {
      ok(false, "catch error " + err);
    }

    // Ignore notifications we don't care about (like favicons)
    if (channel.URI.spec.indexOf(
          "http://example.org/tests/extensions/cookie/test/") == -1) {
      ok(true, "ignoring this one");
      return;
    }

    this.window.isnot(channel.getRequestHeader("Cookie").indexOf("oh=hai"), -1,
                      "cookie 'oh=hai' is in header for " + channel.URI.spec);
    ++gHeaders;
  },

  remove: function obs_remove()
  {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    ok(true, "removing observer");

    this.os.removeObserver(this, "http-on-modify-request");
    this.os = null;
    this.window = null;
  }
}

/** Receives MessageEvents to this window. */
// Count and check loads.
function messageReceiver(evt)
{
  ok(evt.data == "f_lf_i msg data img" || evt.data == "f_lf_i msg data page",
     "message data received from popup");
  if (evt.data == "f_lf_i msg data img") {
    ok(true, "message data received from popup for image");
  }
  if (evt.data == "f_lf_i msg data page") {
    ok(true, "message data received from popup for page");
  }
  if (evt.data != "f_lf_i msg data img" && evt.data != "f_lf_i msg data page") {
    ok(true, "got this message but don't know what it is " + evt.data);
    gPopup.close();
    window.removeEventListener("message", messageReceiver, false);

    finishTest();
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

  is(gHeaders, gExpectedHeaders, "number of observed request headers");

  var cs = SpecialPowers.Cc["@mozilla.org/cookiemanager;1"]
                        .getService(SpecialPowers.Ci.nsICookieManager);
  var count = 0;
  var list = cs.enumerator;
  while (list.hasMoreElements()) {
    var cookie = list.getNext().QueryInterface(SpecialPowers.Ci.nsICookie);
    ok(true, "cookie: " + cookie);
    ok(true, "cookie host " + cookie.host + " path " + cookie.path + " name " + cookie.name +
       " value " + cookie.value + " isSecure " + cookie.isSecure + " expires " + cookie.expires);
    ++count;
  }
  is(count, gExpectedCookies, "total number of cookies");
  cs.removeAll();

  finishTest();
}
