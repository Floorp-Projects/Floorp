// Custom *.sjs file specifically for the needs of Bug:
// Bug 1139297 - Implement CSP upgrade-insecure-requests directive

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  var queryString = request.queryString;

  // (1) lets process the queryresult request async and
  // wait till we have received the image request.
  if (queryString == "queryresult") {
    response.processAsync();
    setObjectState("queryResult", response);
    return;
  }

  // (2) Handle the image request and return the referrer
  // result back to the stored queryresult request.
  if (request.queryString == "img") {
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);

    let referrer = "";
    try {
      referrer = request.getHeader("referer");
    } catch (e) {
      referrer = "";
    }
    // make sure the received image request was upgraded to https,
    // otherwise we return not only the referrer but also indicate
    // that the request was not upgraded to https. Note, that
    // all upgrades happen in the browser before any non-secure
    // request hits the wire.
    referrer += (request.scheme == "https") ?
                 "" : " but request is not https";

    getObjectState("queryResult", function(queryResponse) {
      if (!queryResponse) {
        return;
      }
      queryResponse.write(referrer);
      queryResponse.finish();
    });
    return;
  }

  // we should not get here ever, but just in case return
  // something unexpected.
  response.write("doh!");
}
