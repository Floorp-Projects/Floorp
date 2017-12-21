var Cu = Components.utils;
Cu.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  var feedFeedURI = Services.io.newURI("feed://example.com/feed.xml");
  var httpFeedURI = Services.io.newURI("feed:http://example.com/feed.xml");
  var httpURI = Services.io.newURI("http://example.com/feed.xml");

  var httpsFeedURI =
    Services.io.newURI("feed:https://example.com/feed.xml");
  var httpsURI = Services.io.newURI("https://example.com/feed.xml");

  var feedChannel = NetUtil.newChannel({
    uri: feedFeedURI,
    loadUsingSystemPrincipal: true
  });

  var httpChannel = NetUtil.newChannel({
    uri: httpFeedURI,
    loadUsingSystemPrincipal: true
  });

  var httpsChannel = NetUtil.newChannel({
    uri: httpsFeedURI,
    loadUsingSystemPrincipal: true
  });

  // not setting .originalURI to the original URI is naughty
  Assert.ok(feedFeedURI.equals(feedChannel.originalURI));
  Assert.ok(httpFeedURI.equals(httpChannel.originalURI));
  Assert.ok(httpsFeedURI.equals(httpsChannel.originalURI));

  // actually using the horrible mess that's a feed: URI is suicidal
  Assert.ok(httpURI.equals(feedChannel.URI));
  Assert.ok(httpURI.equals(httpChannel.URI));
  Assert.ok(httpsURI.equals(httpsChannel.URI));

  // check that we throw creating feed: URIs from file and ftp
  Assert.throws(function() { Services.io.newURI("feed:ftp://example.com/feed.xml"); },
      "Should throw an exception when trying to create a feed: URI with an ftp: inner");
  Assert.throws(function() { Services.io.newURI("feed:file:///var/feed.xml"); },
      "Should throw an exception when trying to create a feed: URI with a file: inner");
}
