// Custom *.sjs file specifically for the needs of Bug 1286861

// small red image
const IMG = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

function getSniffableContent(selector){
  switch(selector){
  case "xml":
    return `<?xml version="1.0"?><test/>`;
  case "html":
    return `<!Doctype html> <html> <head></head> <body> Test test </body></html>`;
  case "css":
    return `*{ color: pink !important; }`;
  case 'json':
      return `{ 'test':'yes' }`;
  case 'img':
      return IMG;
  }
  return "Basic UTF-8 Text";
}

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader('X-Content-Type-Options', 'nosniff'); // Disable Sniffing
  response.setHeader("Content-Type","*/*");  // Try Browser to force sniffing. 
  response.write(getSniffableContent(request.queryString));
  return;
}

