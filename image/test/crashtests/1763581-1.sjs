
function getFileStream(filename)
{
  // Get the location of this sjs file, and then use that to figure out where
  // to find where our other files are.
  var self = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsIFile);
  self.initWithPath(getState("__LOCATION__"));
  var file = self.parent;
  file.append(filename);

  var fileStream = Components.classes['@mozilla.org/network/file-input-stream;1']
                     .createInstance(Components.interfaces.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);

  return fileStream;
}

// stolen from file_blocked_script.sjs
function setGlobalState(data, key) {
  x = {
    data,
    QueryInterface: ChromeUtils.generateQI([]),
  };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key) {
  var data;
  getObjectState(key, function(x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

const DELAY_MS = 100;
let gTimer;

function handleRequest(request, response) {
  let count = getGlobalState("count");
  if (count == null) {
    count = 0;
  }

  if (count > 0) {
    response.processAsync();

    gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    gTimer.init(
      () => {
        response.setStatusLine(request.httpVersion, 304, "Not Modified");
        response.finish();
      },
      DELAY_MS,
      Ci.nsITimer.TYPE_ONE_SHOT
    );

    return;
  }

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/gif", false);

  var inputStream = getFileStream("1763581-1.gif");
  response.bodyOutputStream.writeFrom(inputStream, inputStream.available());
  inputStream.close();

  count++;
  setGlobalState(count, "count");
}
