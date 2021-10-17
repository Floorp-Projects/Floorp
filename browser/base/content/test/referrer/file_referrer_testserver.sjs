/**
 * Renders the HTTP Referer header up to the second path slash.
 * Used in browser_referrer_*.js, bug 1113431.
 */
function handleRequest(request, response)
{
  let referrer = "";
  try {
    referrer = request.getHeader("referer");
  } catch (e) {
    referrer = "";
  }

  // Strip it past the first path slash. Makes tests easier to read.
  referrer = referrer.split("/").slice(0, 4).join("/");

  let html = `<!DOCTYPE HTML>
             <html>
             <head>
             <meta charset='utf-8'>
             <title>Test referrer</title>
             </head>
             <body>
             <div id='testdiv'>${referrer}</div>
             </body>
             </html>`;

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.write(html);
}
