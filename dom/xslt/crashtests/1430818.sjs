function getFileStream(filename)
{
  let self = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsIFile);
  self.initWithPath(getState("__LOCATION__"));
  let file = self.parent;
  file.append(filename);
  let stream  = Components.classes['@mozilla.org/network/file-input-stream;1']
                          .createInstance(Components.interfaces.nsIFileInputStream);
  stream.init(file, -1, -1, false);
  return stream;
}

function handleRequest(request, response)
{
  response.processAsync();
  response.setStatusLine(null, 200, "OK");
  response.setHeader("Content-Type", "text/xml", false);

  switch (request.queryString) {
    case "stylesheet":
    {
      let timer = Components.classes["@mozilla.org/timer;1"]
                            .createInstance(Components.interfaces.nsITimer);
      timer.initWithCallback(() => {
        setState("xslt", "loaded");
        response.finish();
        timer.cancel();
      }, 1000 /* milliseconds */, Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);
      break;
    }
    default:
    {
      let stream = getFileStream("1430818.xml");
      response.bodyOutputStream.writeFrom(stream,
                                          stream.available());
      stream.close();
      let timer = Components.classes["@mozilla.org/timer;1"]
                            .createInstance(Components.interfaces.nsITimer);
      timer.initWithCallback(() => {
        if (getState("xslt") == "loaded") {
          response.finish();
          timer.cancel();
        }
      }, 100 /* milliseconds */, Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);
      break;
    }
  }
}
