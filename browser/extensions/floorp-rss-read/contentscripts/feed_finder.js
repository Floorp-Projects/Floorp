"use strict";

// First check to see if this document is a feed. If so, it will redirect.
// Otherwise, check if it has embedded feed links, such as:
// (<link rel="alternate" type="application/rss+xml" etc). If so, show the
// page action icon.

findFeedLinks();

// See if the document contains a <link> tag within the <head> and
// whether that points to an RSS feed.
function findFeedLinks() {
  // Find all the RSS link elements.
  const result = document.evaluate(
    '//*[local-name()="link"][contains(@rel, "alternate")] ' +
      '[contains(@type, "rss") or contains(@type, "atom") or ' +
      'contains(@type, "rdf")]', document, null, 0, null);

  const feeds = [];
  let item;
  while ((item = result.iterateNext())) {
    feeds.push({"href": item.href, "title": item.title});
  }

  if (feeds.length > 0) {
    // Notify the extension needs to show the RSS page action icon.
    chrome.runtime.sendMessage({msg: "feeds", feeds});
  }
}
