// Send a payload that's over 32k in size, which for a plain response should be
// large enough to ensure that OnDataAvailable is called more than once (and so
// the XHR will be triggered to send more than one "loading" event if desired).

function handleRequest(request, response)
{
  let data = (new Array(1 << 18)).join("A");
  response.processAsync();
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Length", "" + data.length, false);
  response.write(data, data.length);
  response.finish();
}
