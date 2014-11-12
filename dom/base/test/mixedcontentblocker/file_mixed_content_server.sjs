
function handleRequest(request, response)
{
  // get the Content-Type to serve from the query string
  var contentType = null;
  request.queryString.split('&').forEach( function (val) {
     var [name, value] = val.split('=');
       if (name == "type") {
         contentType = unescape(value);
       }
  });

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  switch (contentType) {
    case "iframe":
      response.setHeader("Content-Type", "text/html", false);
      response.write("frame content");
      break;

    case "script":
      response.setHeader("Content-Type", "application/javascript", false);
      break;

    case "stylesheet":
      response.setHeader("Content-Type", "text/css", false);
      break;

    case "object":
      response.setHeader("Content-Type", "application/x-test", false);
      break;

   case "xhr":
      response.setHeader("Content-Type", "text/xml", false);
      response.setHeader("Access-Control-Allow-Origin", "https://example.com");
      response.write('<?xml version="1.0" encoding="UTF-8" ?><test></test>');
      break;

    default:
      response.setHeader("Content-Type", "text/html", false);
      response.write("<html><body>Hello World</body></html>");
      break;
  }
}
