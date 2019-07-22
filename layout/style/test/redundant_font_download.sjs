const BinaryOutputStream =
  Components.Constructor("@mozilla.org/binaryoutputstream;1",
                         "nsIBinaryOutputStream",
                         "setOutputStream");

// this is simply a hex dump of a red square .PNG image
const RED_SQUARE =
  [
    0x89,  0x50,  0x4E,  0x47,  0x0D,  0x0A,  0x1A,  0x0A,  0x00,  0x00,
    0x00,  0x0D,  0x49,  0x48,  0x44,  0x52,  0x00,  0x00,  0x00,  0x20,
    0x00,  0x00,  0x00,  0x20,  0x08,  0x02,  0x00,  0x00,  0x00,  0xFC,
    0x18,  0xED,  0xA3,  0x00,  0x00,  0x00,  0x01,  0x73,  0x52,  0x47,
    0x42,  0x00,  0xAE,  0xCE,  0x1C,  0xE9,  0x00,  0x00,  0x00,  0x28,
    0x49,  0x44,  0x41,  0x54,  0x48,  0xC7,  0xED,  0xCD,  0x41,  0x0D,
    0x00,  0x00,  0x08,  0x04,  0xA0,  0xD3,  0xFE,  0x9D,  0x35,  0x85,
    0x0F,  0x37,  0x28,  0x40,  0x4D,  0x6E,  0x75,  0x04,  0x02,  0x81,
    0x40,  0x20,  0x10,  0x08,  0x04,  0x02,  0xC1,  0x93,  0x60,  0x01,
    0xA3,  0xC4,  0x01,  0x3F,  0x58,  0x1D,  0xEF,  0x27,  0x00,  0x00,
    0x00,  0x00,  0x49,  0x45,  0x4E,  0x44,  0xAE,  0x42,  0x60,  0x82
  ];

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  response.setHeader("Cache-Control", "no-cache");

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);

  var log = getState("bug-879963-request-log") || "";

  var stream = new BinaryOutputStream(response.bodyOutputStream);

  if (query["q"] == "init") {
    log = "init"; // initialize the log, and return a PNG image
    response.setHeader("Content-Type", "image/png", false);
    stream.writeByteArray(RED_SQUARE, RED_SQUARE.length);
  } else if (query["q"] == "image") {
    log = log + ";" + query["q"];
    response.setHeader("Content-Type", "image/png", false);
    stream.writeByteArray(RED_SQUARE, RED_SQUARE.length);
  } else if (query["q"] == "font") {
    log = log + ";" + query["q"];
    // we don't provide a real font; that's ok, OTS will just reject it
    response.write("Junk");
  } else if (query["q"] == "report") {
    // don't include the actual "report" request in the log we return
    response.write(log);
  } else {
    log = log + ";" + query["q"];
    response.setStatusLine(request.httpVersion, 404, "Not Found");
  }

  setState("bug-879963-request-log", log);
}
