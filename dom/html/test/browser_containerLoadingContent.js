/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);

const ORIGIN = "https://example.com";
const CROSSORIGIN = "https://example.org";

const TABURL = `${ORIGIN}/${DIRPATH}dummy_page.html`;

const IMAGEURL = `${ORIGIN}/${DIRPATH}image.png`;
const CROSSIMAGEURL = `${CROSSORIGIN}/${DIRPATH}image.png`;

const DOCUMENTURL = `${ORIGIN}/${DIRPATH}dummy_page.html`;
const CROSSDOCUMENTURL = `${CROSSORIGIN}/${DIRPATH}dummy_page.html`;

function getPids(browser) {
  return browser.browsingContext.children.map(
    child => child.currentWindowContext.osPid
  );
}

async function runTest(spec, tabUrl, imageurl, crossimageurl, check) {
  await BrowserTestUtils.withNewTab(tabUrl, async browser => {
    await SpecialPowers.spawn(
      browser,
      [spec, imageurl, crossimageurl],
      async ({ element, attribute }, url1, url2) => {
        for (let url of [url1, url2]) {
          const object = content.document.createElement(element);
          object[attribute] = url;
          const onloadPromise = new Promise(res => {
            object.onload = res;
          });
          content.document.body.appendChild(object);
          await onloadPromise;
        }
      }
    );

    await check(browser);
  });
}

let iframe = { element: "iframe", attribute: "src" };
let embed = { element: "embed", attribute: "src" };
let object = { element: "object", attribute: "data" };

async function checkImage(browser) {
  let pids = getPids(browser);
  is(pids.length, 2, "There should be two browsing contexts");
  ok(pids[0], "The first pid should have a sane value");
  ok(pids[1], "The second pid should have a sane value");
  isnot(pids[0], pids[1], "The two pids should be different");

  let images = [];
  for (let context of browser.browsingContext.children) {
    images.push(
      await SpecialPowers.spawn(context, [], async () => {
        let img = new URL(content.document.querySelector("img").src);
        is(
          `${img.protocol}//${img.host}`,
          `${content.location.protocol}//${content.location.host}`,
          "Images should be loaded in the same domain as the document"
        );
        return img.href;
      })
    );
  }
  isnot(images[0], images[1], "The images should have different sources");
}

function checkDocument(browser) {
  let pids = getPids(browser);
  is(pids.length, 2, "There should be two browsing contexts");
  ok(pids[0], "The first pid should have a sane value");
  ok(pids[1], "The second pid should have a sane value");
  isnot(pids[0], pids[1], "The two pids should be different");
}

add_task(async function test_iframeImageDocument() {
  await runTest(iframe, TABURL, IMAGEURL, CROSSIMAGEURL, checkImage);
});

add_task(async function test_embedImageDocument() {
  await runTest(embed, TABURL, IMAGEURL, CROSSIMAGEURL, checkImage);
});

add_task(async function test_objectImageDocument() {
  await runTest(object, TABURL, IMAGEURL, CROSSIMAGEURL, checkImage);
});

add_task(async function test_iframeDocument() {
  await runTest(iframe, TABURL, DOCUMENTURL, CROSSDOCUMENTURL, checkDocument);
});

add_task(async function test_embedDocument() {
  await runTest(embed, TABURL, DOCUMENTURL, CROSSDOCUMENTURL, checkDocument);
});

add_task(async function test_objectDocument() {
  await runTest(object, TABURL, DOCUMENTURL, CROSSDOCUMENTURL, checkDocument);
});
