// Send a payload that's over 32k in size, which for a plain response should be
// large enough to ensure that OnDataAvailable is called more than once (and so
// the XHR will be triggered to send more than one "loading" event if desired).

function handleRequest(request, response)
{
  // Send 81920 bytes of こんにちは in Shift-JIS encoding, framed in XML.
  let data = "<?xml version='1.0' encoding='shift-jis'?><xml>" +
             (new Array(1 << 13)).join("\x82\xb1\x82\xf1\x82\xc9\x82\xbf\x82\xcd") +
             "</xml>";
  response.processAsync();
  response.setHeader("Content-Type", "text/xml", false);
  response.setHeader("Content-Length", "" + data.length, false);
  response.write(data, data.length);
  response.finish();
}
