const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1", "nsIBinaryInputStream", "setInputStream");

// Returns a JSON string containing the query string arguments and the
// request body parsed as JSON.
function handleRequest(request, response) {
  // Allow cross-origin, so you can XHR to it!
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  // Avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/json", false);

  // Read request body
  const inputStream = new BinaryInputStream(request.bodyInputStream);
  let bytes = [];
  let available;
  while ((available = inputStream.available()) > 0) {
    bytes = bytes.concat(inputStream.readByteArray(available));
  }
  const body = String.fromCharCode.apply(null, bytes);

  // Write response body
  const data = {queryString: {}, body: body ? JSON.parse(body) : {}};
  const params = request.queryString.split("&");
  for (const param of params) {
    const [key, value] = param.split("=");
    data.queryString[key] = value;
  }
  response.write(JSON.stringify(data));
}
