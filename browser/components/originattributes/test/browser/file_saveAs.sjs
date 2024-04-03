const HTTP_ORIGIN = "http://example.com";
const SECOND_ORIGIN = "http://example.org";
const URI_PATH = "/browser/browser/components/originattributes/test/browser/";
const LINK_PATH = `${URI_PATH}file_saveAs.sjs`;
// Reusing existing ogv file for testing.
const VIDEO_PATH = `${URI_PATH}file_thirdPartyChild.video.ogv`;
// Reusing existing png file for testing.
const IMAGE_PATH = `${URI_PATH}file_favicon.png`;
const FRAME_PATH = `${SECOND_ORIGIN}${URI_PATH}file_saveAs.sjs?image=1`;

function handleRequest(aRequest, aResponse) {
  var params = new URLSearchParams(aRequest.queryString);
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  aResponse.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  let contentBody = "";

  if (params.has("link")) {
    contentBody = `<a href="${LINK_PATH}" id="link1">this is a link</a>`;
  } else if (params.has("video")) {
    contentBody = `<video src="${VIDEO_PATH}" id="video1"> </video>`;
  } else if (params.has("image")) {
    contentBody = `<img src="${IMAGE_PATH}" id="image1">`;
  } else if (params.has("page")) {
    // We need at least one resource, like a img, a link or a script, to trigger
    // downloading resources in "Save Page As". Otherwise, it will output the
    // document directly without any network request.
    contentBody = `<img src="${IMAGE_PATH}">`;
  } else if (params.has("frame")) {
    // Like "Save Page As", we need to put at least one resource in the frame.
    // Here we also use a image.
    contentBody = `<iframe src="${FRAME_PATH}" id="frame1"></iframe>`;
  } else if (params.has("pageinfo")) {
    contentBody = `<img src="${IMAGE_PATH}" id="image1">
                   <video src="${VIDEO_PATH}" id="video1"> </video>`;
  }

  aResponse.write(`<html><body>${contentBody}</body></html>`);
}
