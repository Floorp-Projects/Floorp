/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the list of grids does refresh when targets are added or removed (e.g. when
// there's a navigation and iframe are added or removed)

add_task(async function () {
  await addTab(getDocumentBuilderUrl("example.com", "top-level-com-grid"));
  const { gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;

  const checkGridList = (expected, assertionMessage) =>
    checkGridListItems(doc, expected, assertionMessage);

  checkGridList(
    ["div#top-level-com-grid"],
    "One grid item is displayed at first"
  );

  info(
    "Check that adding same-origin iframe with a grid will update the grid list"
  );
  const sameOriginIframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [getDocumentBuilderUrl("example.com", "iframe-com-grid")],
    src => {
      const iframe = content.document.createElement("iframe");
      iframe.id = "same-origin";
      iframe.src = src;
      content.document.body.append(iframe);
      return iframe.browsingContext;
    }
  );

  await waitFor(() => getGridListItems(doc).length == 2);
  checkGridList(
    ["div#top-level-com-grid", "div#iframe-com-grid"],
    "The same-origin iframe grid is displayed"
  );

  info("Check that adding remote iframe with a grid will update the grid list");
  const remoteIframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [getDocumentBuilderUrl("example.org", "iframe-org-grid")],
    src => {
      const iframe = content.document.createElement("iframe");
      iframe.id = "remote";
      iframe.src = src;
      content.document.body.append(iframe);
      return iframe.browsingContext;
    }
  );

  await waitFor(() => getGridListItems(doc).length == 3);
  checkGridList(
    ["div#top-level-com-grid", "div#iframe-com-grid", "div#iframe-org-grid"],
    "The remote iframe grid is displayed"
  );

  info("Check that adding new grids in iframes does update the grid list");
  SpecialPowers.spawn(sameOriginIframeBrowsingContext, [], () => {
    const section = content.document.createElement("section");
    section.id = "com-added-grid-container";
    section.style = "display: grid;";
    content.document.body.append(section);
  });

  await waitFor(() => getGridListItems(doc).length == 4);
  checkGridList(
    [
      "div#top-level-com-grid",
      "div#iframe-com-grid",
      "section#com-added-grid-container",
      "div#iframe-org-grid",
    ],
    "The new grid in the same origin iframe is displayed"
  );

  SpecialPowers.spawn(remoteIframeBrowsingContext, [], () => {
    const section = content.document.createElement("section");
    section.id = "org-added-grid-container";
    section.style = "display: grid;";
    content.document.body.append(section);
  });

  await waitFor(() => getGridListItems(doc).length == 5);
  checkGridList(
    [
      "div#top-level-com-grid",
      "div#iframe-com-grid",
      "section#com-added-grid-container",
      "div#iframe-org-grid",
      "section#org-added-grid-container",
    ],
    "The new grid in the same origin iframe is displayed"
  );

  info("Check that removing iframes will update the grid list");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("iframe#same-origin").remove();
  });

  await waitFor(() => getGridListItems(doc).length == 3);
  checkGridList(
    [
      "div#top-level-com-grid",
      "div#iframe-org-grid",
      "section#org-added-grid-container",
    ],
    "The same-origin iframe grids were removed from the list"
  );

  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("iframe#remote").remove();
  });

  await waitFor(() => getGridListItems(doc).length == 1);
  checkGridList(
    ["div#top-level-com-grid"],
    "The remote iframe grids were removed as well"
  );

  info("Navigate to a new origin");
  await navigateTo(getDocumentBuilderUrl("example.org", "top-level-org-grid"));
  await waitFor(() => {
    const listItems = getGridListItems(doc);
    return (
      listItems.length == 1 &&
      listItems[0].textContent.includes("#top-level-org-grid")
    );
  });
  checkGridList(
    ["div#top-level-org-grid"],
    "The grid from the new origin document is displayed"
  );

  info("Check that adding remote iframe will still update the grid list");
  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [getDocumentBuilderUrl("example.com", "iframe-com-grid-remote")],
    src => {
      const iframe = content.document.createElement("iframe");
      iframe.id = "remote";
      iframe.src = src;
      content.document.body.append(iframe);
    }
  );

  await waitFor(() => getGridListItems(doc).length == 2);
  checkGridList(
    ["div#top-level-org-grid", "div#iframe-com-grid-remote"],
    "The grid from the new origin document is displayed"
  );

  info(
    "Check that adding same-origin iframe with a grid will update the grid list"
  );
  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [getDocumentBuilderUrl("example.org", "iframe-org-grid-same-origin")],
    src => {
      const iframe = content.document.createElement("iframe");
      iframe.id = "same-origin";
      iframe.src = src;
      content.document.body.append(iframe);
    }
  );
  await waitFor(() => getGridListItems(doc).length == 3);
  checkGridList(
    [
      "div#top-level-org-grid",
      "div#iframe-com-grid-remote",
      "div#iframe-org-grid-same-origin",
    ],
    "The grid from the new same-origin iframe is displayed"
  );
});

function getDocumentBuilderUrl(origin, gridContainerId) {
  return `https://${origin}/document-builder.sjs?html=${encodeURIComponent(
    `<style>
      #${gridContainerId} {
        display: grid;
      }
    </style>
    <div id="${gridContainerId}"></div>`
  )}`;
}

function getGridListItems(doc) {
  return Array.from(doc.querySelectorAll("#grid-list .objectBox-node"));
}

function checkGridListItems(doc, expectedItems, assertionText) {
  const gridItems = getGridListItems(doc).map(el => el.textContent);
  is(
    JSON.stringify(gridItems.sort()),
    JSON.stringify(expectedItems.sort()),
    assertionText
  );
}
