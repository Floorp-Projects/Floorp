var Cu = Components.utils;
Cu.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  var feedFeedURI = ios.newURI("feed://example.com/feed.xml", null, null);
  var httpFeedURI = ios.newURI("feed:http://example.com/feed.xml", null, null);
  var httpURI = ios.newURI("http://example.com/feed.xml", null, null);

  var httpsFeedURI =
    ios.newURI("feed:https://example.com/feed.xml", null, null);
  var httpsURI = ios.newURI("https://example.com/feed.xml", null, null);

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
  do_check_true(feedFeedURI.equals(feedChannel.originalURI));
  do_check_true(httpFeedURI.equals(httpChannel.originalURI));
  do_check_true(httpsFeedURI.equals(httpsChannel.originalURI));

  // actually using the horrible mess that's a feed: URI is suicidal
  do_check_true(httpURI.equals(feedChannel.URI));
  do_check_true(httpURI.equals(httpChannel.URI));
  do_check_true(httpsURI.equals(httpsChannel.URI));

  // check that we throw creating feed: URIs from file and ftp
  Assert.throws(function() { ios.newURI("feed:ftp://example.com/feed.xml", null, null); },
      "Should throw an exception when trying to create a feed: URI with an ftp: inner");
  Assert.throws(function() { ios.newURI("feed:file:///var/feed.xml", null, null); },
      "Should throw an exception when trying to create a feed: URI with a file: inner");
}
