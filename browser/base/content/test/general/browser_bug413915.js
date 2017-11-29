XPCOMUtils.defineLazyModuleGetter(this, "Feeds",
  "resource:///modules/Feeds.jsm");

function test() {
  var exampleUri = makeURI("http://example.com/");
  var principal = Services.scriptSecurityManager.createCodebasePrincipal(exampleUri, {});

  function testIsFeed(aTitle, aHref, aType, aKnown) {
    var link = {
      title: aTitle,
      href: aHref,
      type: aType,
      ownerDocument: {
        characterSet: "UTF-8"
      }
    };
    return Feeds.isValidFeed(link, principal, aKnown);
  }

  var href = "http://example.com/feed/";
  var atomType = "application/atom+xml";
  var funkyAtomType = " aPPLICAtion/Atom+XML ";
  var rssType = "application/rss+xml";
  var funkyRssType = " Application/RSS+XML  ";
  var rdfType = "application/rdf+xml";
  var texmlType = "text/xml";
  var appxmlType = "application/xml";
  var noRss = "Foo";
  var rss = "RSS";

  // things that should be valid
  ok(testIsFeed(noRss, href, atomType, false) == atomType,
     "detect Atom feed");
  ok(testIsFeed(noRss, href, funkyAtomType, false) == atomType,
     "clean up and detect Atom feed");
  ok(testIsFeed(noRss, href, rssType, false) == rssType,
     "detect RSS feed");
  ok(testIsFeed(noRss, href, funkyRssType, false) == rssType,
     "clean up and detect RSS feed");

  // things that should not be feeds
  ok(testIsFeed(noRss, href, rdfType, false) == null,
     "should not detect RDF non-feed");
  ok(testIsFeed(rss, href, rdfType, false) == null,
     "should not detect RDF feed from type and title");
  ok(testIsFeed(noRss, href, texmlType, false) == null,
     "should not detect text/xml non-feed");
  ok(testIsFeed(rss, href, texmlType, false) == null,
     "should not detect text/xml feed from type and title");
  ok(testIsFeed(noRss, href, appxmlType, false) == null,
     "should not detect application/xml non-feed");
  ok(testIsFeed(rss, href, appxmlType, false) == null,
     "should not detect application/xml feed from type and title");

  // security check only, returns cleaned up type or "application/rss+xml"
  ok(testIsFeed(noRss, href, atomType, true) == atomType,
     "feed security check should return Atom type");
  ok(testIsFeed(noRss, href, funkyAtomType, true) == atomType,
     "feed security check should return cleaned up Atom type");
  ok(testIsFeed(noRss, href, rssType, true) == rssType,
     "feed security check should return RSS type");
  ok(testIsFeed(noRss, href, funkyRssType, true) == rssType,
     "feed security check should return cleaned up RSS type");
  ok(testIsFeed(noRss, href, "", true) == rssType,
     "feed security check without type should return RSS type");
  ok(testIsFeed(noRss, href, "garbage", true) == "garbage",
     "feed security check with garbage type should return garbage");
}
