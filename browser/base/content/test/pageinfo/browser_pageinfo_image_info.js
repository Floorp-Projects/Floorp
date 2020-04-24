/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 **/

const URI =
  "data:text/html," +
  "<style type='text/css'>%23test-image,%23not-test-image {background-image: url('about:logo?c');}</style>" +
  "<img src='about:logo?b' height=300 width=350 alt=2 id='not-test-image'>" +
  "<img src='about:logo?b' height=300 width=350 alt=2>" +
  "<img src='about:logo?a' height=200 width=250>" +
  "<img src='about:logo?b' height=200 width=250 alt=1>" +
  "<img src='about:logo?b' height=100 width=150 alt=2 id='test-image'>";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URI);
  let browser = tab.linkedBrowser;

  let imageInfo = await SpecialPowers.spawn(browser, [], async () => {
    let testImg = content.document.getElementById("test-image");

    return {
      src: testImg.src,
      currentSrc: testImg.currentSrc,
      width: testImg.width,
      height: testImg.height,
      imageText: testImg.title || testImg.alt,
    };
  });

  let pageInfo = BrowserPageInfo(
    browser.currentURI.spec,
    "mediaTab",
    imageInfo
  );
  await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");

  let pageInfoImg = pageInfo.document.getElementById("thepreviewimage");
  await BrowserTestUtils.waitForEvent(pageInfoImg, "loadend");
  Assert.equal(
    pageInfoImg.src,
    imageInfo.src,
    "selected image has the correct source"
  );
  Assert.equal(
    pageInfoImg.width,
    imageInfo.width,
    "selected image has the correct width"
  );
  Assert.equal(
    pageInfoImg.height,
    imageInfo.height,
    "selected image has the correct height"
  );
  pageInfo.close();
  BrowserTestUtils.removeTab(tab);
});
