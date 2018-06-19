/* Make sure that "View Image Info" loads the correct image data */
function getImageInfo(imageElement) {
  return {
    currentSrc: imageElement.currentSrc,
    width: imageElement.width,
    height: imageElement.height,
    imageText: imageElement.title || imageElement.alt
  };
}

const URI =
  "data:text/html," +
  "<style type='text/css'>%23test-image,%23not-test-image {background-image: url('about:logo?c');}</style>" +
  "<img src='about:logo?b' height=300 width=350 alt=2 id='not-test-image'>" +
  "<img src='about:logo?b' height=300 width=350 alt=2>" +
  "<img src='about:logo?a' height=200 width=250>" +
  "<img src='about:logo?b' height=200 width=250 alt=1>" +
  "<img src='about:logo?b' height=100 width=150 alt=2 id='test-image'>";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false,
                                 URI).then(function() {
    var doc = gBrowser.contentDocumentAsCPOW;
    var testImg = doc.getElementById("test-image");
    var pageInfo = BrowserPageInfo(gBrowser.selectedBrowser.currentURI.spec,
                                   "mediaTab", getImageInfo(testImg));

    pageInfo.addEventListener("load", function() {
      pageInfo.onFinished.push(function() {
        var pageInfoImg = pageInfo.document.getElementById("thepreviewimage");
        pageInfoImg.addEventListener("loadend", function() {

          is(pageInfoImg.src, testImg.src, "selected image has the correct source");
          is(pageInfoImg.width, testImg.width, "selected image has the correct width");
          is(pageInfoImg.height, testImg.height, "selected image has the correct height");

          pageInfo.close();
          gBrowser.removeCurrentTab();
          finish();
        });
      });
    }, {capture: true, once: true});
  });

  gBrowser.loadURI(URI);
}
