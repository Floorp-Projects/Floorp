/*
 * Custom sjs file serving a test page using *two* CSP policies.
 * See Bug 1036399 - Multiple CSP policies should be combined towards an intersection
 */

const TIGHT_POLICY = "default-src 'self'";
const LOOSE_POLICY = "default-src 'self' 'unsafe-inline'";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  var csp = "";
  // deliver *TWO* comma separated policies which is in fact the same as serving
  // to separate CSP headers (AppendPolicy is called twice).
  if (request.queryString == "tight") {
    // script execution will be *blocked*
    csp = TIGHT_POLICY + ", " + LOOSE_POLICY;
  }
  else {
        // script execution will be *allowed*
    csp = LOOSE_POLICY + ", " + LOOSE_POLICY;
  }
  response.setHeader("Content-Security-Policy", csp, false);

  // Send HTML to test allowed/blocked behaviors
  response.setHeader("Content-Type", "text/html", false);

  // generate an html file that contains a div container which is updated
  // in case the inline script is *not* blocked by CSP.
  var html = "<!DOCTYPE HTML>" +
             "<html>" +
             "<head>" +
             "<title>Testpage for Bug 1036399</title>" +
             "</head>" +
             "<body>" +
             "<div id='testdiv'>blocked</div>" +
             "<script type='text/javascript'>" +
             "document.getElementById('testdiv').innerHTML = 'allowed';" +
             "</script>" +
             "</body>" +
             "</html>";

  response.write(html);
}
