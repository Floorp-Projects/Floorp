/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

"use strict";

function testLinkIndexAtOffset(id, offset, index) {
  let htAcc = getAccessible(id, [nsIAccessibleHyperText]);
  is(
    htAcc.getLinkIndexAtOffset(offset),
    index,
    "Wrong link index at offset " + offset + " for ID " + id + "!"
  );
}

function testThis(
  paragraph,
  docURI,
  id,
  charIndex,
  expectedLinkIndex,
  expectedAnchors,
  expectedURIs,
  valid = true
) {
  testLinkIndexAtOffset(paragraph, charIndex, expectedLinkIndex);

  let linkAcc = paragraph.getLinkAt(expectedLinkIndex);
  ok(linkAcc, "No accessible for link " + id + "!");

  is(linkAcc.valid, valid, `${id} is valid.`);

  let linkIndex = paragraph.getLinkIndex(linkAcc);
  is(linkIndex, expectedLinkIndex, "Wrong link index for " + id + "!");

  is(linkAcc.anchorCount, expectedAnchors.length, "Correct number of anchors");
  for (let i = 0; i < expectedAnchors.length; i++) {
    let uri = linkAcc.getURI(i);
    is(
      (uri ? uri.spec : "").replace(docURI, ""),
      expectedURIs[i],
      `Wrong anchor URI at ${i} for "${id}"`
    );
    is(
      getAccessibleDOMNodeID(linkAcc.getAnchor(i)),
      expectedAnchors[i],
      `Wrong anchor at ${i} for "${id}"`
    );
  }
}

/**
 * Test hyperlinks
 */
addAccessibleTask(
  `
  <p id="testParagraph"><br
  >Simple link:<br
  ><a id="NormalHyperlink" href="https://www.mozilla.org">Mozilla Foundation</a><br
  >ARIA link:<br
  ><span id="AriaHyperlink" role="link"
          onclick="window.open('https://www.mozilla.org/');"
          tabindex="0">Mozilla Foundation Home</span><br
  >Invalid, non-focusable hyperlink:<br
  ><span id="InvalidAriaHyperlink" role="link" aria-invalid="true"
         onclick="window.open('https:/www.mozilla.org/');">Invalid link</span><br
  >Image map:<br
  ><map name="atoz_map"><area href="https://www.bbc.co.uk/radio4/atoz/index.shtml#b"
                              coords="17,0,30,14"
                              id="b"
                              shape="rect"></area
   ><area href="https://www.bbc.co.uk/radio4/atoz/index.shtml#a"
          coords="0,0,13,14"
          id="a"
          shape="rect"></area></map
   ><img width="447" id="imgmap"
         height="15"
         usemap="#atoz_map"
         src="../letters.gif"></img><br
  >Empty link:<br
  ><a id="emptyLink" href=""><img src=""></img></a><br
  >Link with embedded span<br
  ><a id="LinkWithSpan" href="https://www.heise.de/"><span lang="de">Heise Online</span></a><br
  >Named anchor, must not have "linked" state for it to be exposed correctly:<br
  ><a id="namedAnchor" name="named_anchor">This should never be of state_linked</a>
  </p>
  `,
  function (browser, accDoc) {
    const paragraph = findAccessibleChildByID(accDoc, "testParagraph", [
      nsIAccessibleHyperText,
    ]);
    is(paragraph.linkCount, 7, "Wrong link count for paragraph!");

    const docURI = accDoc.URL;
    // normal hyperlink
    testThis(
      paragraph,
      docURI,
      "NormalHyperlink",
      14,
      0,
      ["NormalHyperlink"],
      ["https://www.mozilla.org/"]
    );

    // ARIA hyperlink
    testThis(
      paragraph,
      docURI,
      "AriaHyperlink",
      27,
      1,
      ["AriaHyperlink"],
      [""]
    );

    // ARIA hyperlink with status invalid
    testThis(
      paragraph,
      docURI,
      "InvalidAriaHyperlink",
      63,
      2,
      ["InvalidAriaHyperlink"],
      [""],
      false
    );

    // image map, but not its link children. They are not part of hypertext.
    testThis(
      paragraph,
      docURI,
      "imgmap",
      76,
      3,
      ["b", "a"],
      [
        "https://www.bbc.co.uk/radio4/atoz/index.shtml#b",
        "https://www.bbc.co.uk/radio4/atoz/index.shtml#a",
      ]
    );

    // empty hyperlink
    testThis(paragraph, docURI, "emptyLink", 90, 4, ["emptyLink"], [""]);

    // normal hyperlink with embedded span
    testThis(
      paragraph,
      docURI,
      "LinkWithSpan",
      116,
      5,
      ["LinkWithSpan"],
      ["https://www.heise.de/"]
    );

    // Named anchor
    testThis(paragraph, docURI, "namedAnchor", 193, 6, ["namedAnchor"], [""]);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test paragraph with link
 */
addAccessibleTask(
  `
  <p id="p"><a href="http://mozilla.org">mozilla.org</a></p>
  `,
  function (browser, accDoc) {
    // Paragraph with link
    const p = findAccessibleChildByID(accDoc, "p", [nsIAccessibleHyperText]);
    const link = p.getLinkAt(0);
    is(link, p.getChildAt(0), "Wrong link for p2");
    is(p.linkCount, 1, "Wrong link count for p2");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test paragraph with link
 */
addAccessibleTask(
  `
  <p id="p"><a href="www">mozilla</a><a href="www">mozilla</a><span> te</span><span>xt </span><a href="www">mozilla</a></p>
  `,
  function (browser, accDoc) {
    // Paragraph with link
    const p = findAccessibleChildByID(accDoc, "p", [nsIAccessibleHyperText]);

    // getLinkIndexAtOffset, causes the offsets to be cached;
    testLinkIndexAtOffset(p, 0, 0); // 1st 'mozilla' link
    testLinkIndexAtOffset(p, 1, 1); // 2nd 'mozilla' link
    testLinkIndexAtOffset(p, 2, -1); // ' ' of ' te' text node
    testLinkIndexAtOffset(p, 3, -1); // 't' of ' te' text node
    testLinkIndexAtOffset(p, 5, -1); // 'x' of 'xt ' text node
    testLinkIndexAtOffset(p, 7, -1); // ' ' of 'xt ' text node
    testLinkIndexAtOffset(p, 8, 2); // 3d 'mozilla' link
    testLinkIndexAtOffset(p, 9, 2); // the end, latest link

    // the second pass to make sure link indexes are calculated propertly from
    // cached offsets.
    testLinkIndexAtOffset(p, 0, 0); // 1st 'mozilla' link
    testLinkIndexAtOffset(p, 1, 1); // 2nd 'mozilla' link
    testLinkIndexAtOffset(p, 2, -1); // ' ' of ' te' text node
    testLinkIndexAtOffset(p, 3, -1); // 't' of ' te' text node
    testLinkIndexAtOffset(p, 5, -1); // 'x' of 'xt ' text node
    testLinkIndexAtOffset(p, 7, -1); // ' ' of 'xt ' text node
    testLinkIndexAtOffset(p, 8, 2); // 3d 'mozilla' link
    testLinkIndexAtOffset(p, 9, 2); // the end, latest link
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);
