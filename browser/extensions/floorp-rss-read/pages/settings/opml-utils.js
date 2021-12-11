"use strict";

/* exported exportOPML */
function exportOPML(feeds) {
  const TEMPLATE = `<?xml version="1.0" encoding="UTF-8"?>
<opml version="1.0">
  <head>
    <title>Your livemarks</title>
  </head>
  <body>
    <outline title="RSS Feeds" text="RSS Feeds">
    </outline>
  </body>
</opml>`;

  const parser = new DOMParser();
  const doc = parser.parseFromString(TEMPLATE, "text/xml");

  const container = doc.querySelector("outline");
  for (const { title, feedUrl, siteUrl} of feeds) {
    const outline = doc.createElement("outline");
    outline.setAttribute("text", title);
    outline.setAttribute("title", title);
    outline.setAttribute("type", "rss");
    outline.setAttribute("xmlUrl", feedUrl);
    outline.setAttribute("htmlUrl", siteUrl);
    container.appendChild(outline);
  }

  const serializer = new XMLSerializer();
  return serializer.serializeToString(doc);
}

/* exported importOPML */
function importOPML(opml) {
  const parser = new DOMParser();
  const doc = parser.parseFromString(opml, "text/xml");
  const importedFeeds = [...doc.querySelectorAll("outline[type='rss']")];
  return importedFeeds.map(outline => ({
    "title": outline.getAttribute("text"),
    "feedUrl": outline.getAttribute("xmlUrl"),
    "siteUrl": outline.getAttribute("htmlUrl"),
  }));
}
