const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.write(request.getHeader("Content-Length"));
}
