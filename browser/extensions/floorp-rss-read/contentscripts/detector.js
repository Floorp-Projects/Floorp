"use strict";

/* exported containsFeed */
function containsFeed(doc) {
  debugMsg("containsFeed called");

  // Find all the RSS link elements.
  const result = doc.evaluate(
    '//*[local-name()="rss" or local-name()="feed" or local-name()="RDF"]',
    doc, null, 0, null);

  if (!result) {
    debugMsg("exiting: document.evaluate returned no results");
    return false; // This is probably overly defensive, but whatever.
  }

  const node = result.iterateNext();

  if (!node) {
    debugMsg("returning: iterateNext() returned no nodes");
    return false; // No RSS tags were found.
  }

  // The feed for arab dash jokes dot net, for example, contains
  // a feed that is a child of the body tag so we continue only if the
  // node contains no parent or if the parent is the body tag.
  if (node.parentElement && node.parentElement.tagName != "BODY") {
    debugMsg("exiting: parentElement that's not BODY");
    return false;
  }

  debugMsg("Found feed");

  return true;
}

function debugMsg(text) {
  if (false) {
    console.log("RSS Subscription extension: " + text);
  }
}

// See if the current document is a feed document and if so, go to the
// subscribe page instead.
if (containsFeed(document)) {
  chrome.runtime.sendMessage({msg: "load-preview", url: location.href});
}
