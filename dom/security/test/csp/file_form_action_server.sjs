// Custom *.sjs file specifically for the needs of Bug 1251043

const FRAME = `
  <!DOCTYPE html>
  <html>
  <head>
    <title>Bug 1251043 - Test form-action blocks URL</title>
    <meta http-equiv="Content-Security-Policy" content="form-action 'none';">
  </head>
  <body>
    CONTROL-TEXT
    <form action="file_form_action_server.sjs?formsubmission" method="GET">
      <input type="submit" id="submitButton" value="submit">
    </form>
  </body>
  </html>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // PART 1: Return a frame including the FORM and the CSP
  if (request.queryString === "loadframe") {
    response.write(FRAME);
    return;
  }

  // PART 2: We should never get here because the form
  // should not be submitted. Just in case; return
  // something unexpected so the test fails!
  response.write("do'h");
}
