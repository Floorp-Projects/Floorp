// custom *.sjs for Bug 1255240

const TEST_FRAME = `
  <!DOCTYPE HTML>
  <html>
  <head><meta charset='utf-8'></head>
  <body>
  <a id='testlink' target='innerframe' href='file_contentpolicytype_targeted_link_iframe.sjs?innerframe'>click me</a>
  <iframe name='innerframe'></iframe>
  <script type='text/javascript'>
    var link = document.getElementById('testlink');
    testlink.click();
  </script>
  </body>
  </html> `;

const INNER_FRAME = `
  <!DOCTYPE HTML>
  <html>
  <head><meta charset='utf-8'></head>
  hello world!
  </body>
  </html>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  var queryString = request.queryString;

  if (queryString === "testframe") {
    response.write(TEST_FRAME);
    return;
  }

  if (queryString === "innerframe") {
    response.write(INNER_FRAME);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
