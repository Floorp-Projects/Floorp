"use strict";

/* import-globals-from ../reader/reader.js */
/* import-globals-from ../../shared/i18n.js */

window.addEventListener("load", function() {
  main();
});

async function setPreviewContent(html) {
  // Normal loading just requires links to the css and the js file.
  const frame = document.createElement("iframe");
  frame.classList.add("grow");
  // DANGER! The next line is security critical for this extension!
  // Without this sandbox, malicious feeds can trivially exploit us.
  frame.sandbox = "allow-popups allow-popups-to-escape-sandbox";

  const sheetUrl = chrome.extension.getURL("pages/reader/reader.css");
  const response = await fetch(sheetUrl);
  const text = await response.text();

  frame.srcdoc = `<html>
  <head>
    <meta charset="utf-8">
    <base target="_blank">
    <style type="text/css">${text}</style>
  </head>
  <body>
    ${html}
  </body>
  </html>`;

  document.getElementById("preview").append(frame);
}

/**
* The main function. fetches the feed data.
*/
async function main() {
  const queryString = location.search.substring(1).split("&");
  const feedUrl = decodeURIComponent(queryString[0]);

  document.querySelector("#feed-url").value = feedUrl;
  document.querySelector("#feed-url").addEventListener("focus", (event) => {
    event.target.select();
    document.execCommand("copy");
  });

  const viewSourceBtn = document.querySelector("#view-source-button");
  viewSourceBtn.href = "view-source:" + feedUrl;

  try {
    const feed = await browser.runtime.sendMessage({
      msg: "get-feed",
      feedUrl
    });

    const {title, description, url: siteUrl, items} = feed;
    if (items.length == 0) {
      setPreviewContent(`<main id=\"error\">
                        ${I18N.getMessage("subscribe_noEntriesFound")}
                        </main>`);
      return;
    }

    document.title = title;
    document.querySelector("#title").textContent = title;
    if (description) {
      document.querySelector("#description").textContent = description;
    }

    document.querySelector("#subscribe-button").addEventListener("click", async () => {
      const folderTitle = await browser.runtime.sendMessage({
        msg: "subscribe",
        title,
        feedUrl,
        siteUrl
      });
      alert(I18N.getMessage("subscribe_subscribed", folderTitle));
    });

    setPreviewContent(`<main>${getPreviewHTML(feed)}</main>`);
  } catch (e) {
    console.log(e);
    setPreviewContent(`<main id=\"error\">
                      ${I18N.getMessage("subscribe_failedToFetchFeed")}
                      </main>`);
  }
}
