const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  var feedFeedURI = ios.newURI("feed://example.com/feed.xml", null, null);
  var httpFeedURI = ios.newURI("feed:http://example.com/feed.xml", null, null);
  var httpURI = ios.newURI("http://example.com/feed.xml", null, null);

  var httpsFeedURI =
    ios.newURI("feed:https://example.com/feed.xml", null, null);
  var httpsURI = ios.newURI("https://example.com/feed.xml", null, null);

  var feedChannel = ios.newChannelFromURI2(feedFeedURI,
                                           null,      // aLoadingNode
                                           Services.scriptSecurityManager.getSystemPrincipal(),
                                           null,      // aTriggeringPrincipal
                                           Ci.nsILoadInfo.SEC_NORMAL,
                                           Ci.nsIContentPolicy.TYPE_OTHER);
  var httpChannel = ios.newChannelFromURI2(httpFeedURI,
                                           null,      // aLoadingNode
                                           Services.scriptSecurityManager.getSystemPrincipal(),
                                           null,      // aTriggeringPrincipal
                                           Ci.nsILoadInfo.SEC_NORMAL,
                                           Ci.nsIContentPolicy.TYPE_OTHER);
  var httpsChannel = ios.newChannelFromURI2(httpsFeedURI,
                                            null,      // aLoadingNode
                                            Services.scriptSecurityManager.getSystemPrincipal(),
                                            null,      // aTriggeringPrincipal
                                            Ci.nsILoadInfo.SEC_NORMAL,
                                            Ci.nsIContentPolicy.TYPE_OTHER);

  // not setting .originalURI to the original URI is naughty
  do_check_true(feedFeedURI.equals(feedChannel.originalURI));
  do_check_true(httpFeedURI.equals(httpChannel.originalURI));
  do_check_true(httpsFeedURI.equals(httpsChannel.originalURI));

  // actually using the horrible mess that's a feed: URI is suicidal
  do_check_true(httpURI.equals(feedChannel.URI));
  do_check_true(httpURI.equals(httpChannel.URI));
  do_check_true(httpsURI.equals(httpsChannel.URI));

  // check that we don't throw creating feed: URIs from file and ftp
  var ftpFeedURI = ios.newURI("feed:ftp://example.com/feed.xml", null, null);
  var fileFeedURI = ios.newURI("feed:file:///var/feed.xml", null, null);
}
