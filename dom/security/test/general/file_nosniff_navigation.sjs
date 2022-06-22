// Custom *.sjs file specifically for the needs of Bug 1286861

// small red image
const IMG = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

// https://stackoverflow.com/questions/17279712/what-is-the-smallest-possible-valid-pdf
const PDF = `%PDF-1.0
1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj 2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj 3 0 obj<</Type/Page/MediaBox[0 0 3 3]>>endobj
trailer<</Size 4/Root 1 0 R>>`;

function getSniffableContent(type) {
  switch (type) {
    case "xml":
      return `<?xml version="1.0"?><test/>`;
    case "html":
      return `<!Doctype html> <html> <head></head> <body> Test test </body></html>`;
    case "css":
      return `*{ color: pink !important; }`;
    case "json":
      return `{ 'test':'yes' }`;
    case "img":
      return IMG;
    case "pdf":
      return PDF;
  }
  return "Basic UTF-8 Text";
}

function handleRequest(request, response) {
  Cu.importGlobalProperties(["URLSearchParams"]);
  let query = new URLSearchParams(request.queryString);

  // avoid confusing cache behaviors (XXXX no sure what this means?)
  response.setHeader("X-Content-Type-Options", "nosniff"); // Disable Sniffing
  response.setHeader("Content-Type", query.get("mime"));
  response.write(getSniffableContent(query.get("content")));
}
