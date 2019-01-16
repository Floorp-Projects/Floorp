const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  // This returns number of requests received so far.
  if (aRequest.queryString.includes("result")) {
    let hints = getState("hints") || 0;
    setState("hints", "0");

    let submitter = getState("submitter");
    setState("submitter", "");

    aResponse.write(hints + "-" + submitter);
    return;
  }

  // Here we count the number of requests and we store who was the last
  // submitter.

  let bodyStream = new BinaryInputStream(aRequest.bodyInputStream);
  let requestBody = "";
  while ((bodyAvail = bodyStream.available()) > 0) {
    requestBody += bodyStream.readBytes(bodyAvail);
  }

  let lines = requestBody.split("\n");
  let submitter = "";
  for (let i = 0; i < lines.length; ++i) {
    if (lines[i].trim() == 'Content-Disposition: form-data; name="post"') {
      submitter = lines[i+2].trim();
      break;
    }
  }

  let hints = parseInt(getState("hints") || 0) + 1;
  setState("hints", hints.toString());
  setState("submitter", submitter);

  aResponse.setHeader("Content-Type", "text/html", false);
  aResponse.write("Hello World!");
}
