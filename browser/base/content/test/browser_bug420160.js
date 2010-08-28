var listener = {
  testFunction: null,

  handleEvent: function (e) {
    this.testFunction();
  }
}

/* Tests for correct behaviour of getEffectiveHost on identity handler */
function test() {
  waitForExplicitFinish();

  ok(gIdentityHandler, "gIdentityHandler should exist");

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", listener, true);
  listener.testFunction = testNormalDomain;  
  content.location = "http://test1.example.org/";
}

// Greek IDN for 'example.test'.
var idnDomain = "\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1.\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE";

function testNormalDomain() {
  is(gIdentityHandler._lastLocation.host, 'test1.example.org', "Identity handler is getting the full location");
  is(gIdentityHandler.getEffectiveHost(), 'example.org', "getEffectiveHost should return example.org for test1.example.org");

  listener.testFunction = testIDNDomain;
  content.location = "http://sub1." + idnDomain + "/";
}

function testIDNDomain() {
  is(gIdentityHandler._lastLocation.host, "sub1." + idnDomain, "Identity handler is getting the full location");
  is(gIdentityHandler.getEffectiveHost(), idnDomain, "getEffectiveHost should return the IDN base domain in UTF-8");

  listener.testFunction = testNormalDomainWithPort;
  content.location = "http://sub1.test1.example.org:8000/";
}

function testNormalDomainWithPort() {
  is(gIdentityHandler._lastLocation.host, 'sub1.test1.example.org:8000', "Identity handler is getting port information");
  is(gIdentityHandler.getEffectiveHost(), 'example.org', "getEffectiveHost should return example.org for sub1.test1.example.org:8000");

  listener.testFunction = testIPWithPort;
  content.location = "http://127.0.0.1:8888/";
}

function testIPWithPort() {
  is(gIdentityHandler.getEffectiveHost(), '127.0.0.1', "getEffectiveHost should return 127.0.0.1 for 127.0.0.1:8888");
  gBrowser.selectedBrowser.removeEventListener("load", listener, true);
  gBrowser.removeCurrentTab();
  finish();
}
