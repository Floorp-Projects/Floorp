// Serves a file with a given mime type and size at an optionally given rate.

function getQuery(request) {
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });
  return query;
}

// Timer used to handle the request response.
var timer = null;

function handleResponse() {
  // Is this a rate limited response?
  if (this.state.rate > 0) {
    // Calculate how many bytes we have left to send.
    var bytesToWrite = this.state.totalBytes - this.state.sentBytes;

    // Do we have any bytes left to send? If not we'll just fall thru and
    // cancel our repeating timer and finalize the response.
    if (bytesToWrite > 0) {
      // Figure out how many bytes to send, based on the rate limit.
      bytesToWrite =
        (bytesToWrite > this.state.rate) ? this.state.rate : bytesToWrite;

      for (let i = 0; i < bytesToWrite; i++) {
        this.response.write("0");
      }

      // Update the number of bytes we've sent to the client.
      this.state.sentBytes += bytesToWrite;

      // Wait until the next call to do anything else.
      return;
    }
  }
  else {
    // Not rate limited, write it all out.
    for (let i = 0; i < this.state.totalBytes; i++) {
      this.response.write("0");
    }
  }

  // Finalize the response.
  this.response.finish();

  // All done sending, go ahead and cancel our repeating timer.
  timer.cancel();
}

function handleRequest(request, response) {
  var query = getQuery(request);

  // Default status when responding.
  var version = "1.1";
  var statusCode = 200;
  var description = "OK";

  // Default values for content type, size and rate.
  var contentType = "text/plain";
  var contentRange = null;
  var size = 1024;
  var rate = 0;

  // optional content type to be used by our response.
  if ("contentType" in query) {
    contentType = query["contentType"];
  }

  // optional size (in bytes) for generated file.
  if ("size" in query) {
    size = parseInt(query["size"]);
  }

  // optional range request check.
  if (request.hasHeader("range")) {
    version = "1.1";
    statusCode = 206;
    description = "Partial Content";

    // We'll only support simple range byte style requests.
    var [offset, total] = request.getHeader("range").slice("bytes=".length).split("-");
    // Enforce valid Number values.
    offset = parseInt(offset);
    offset = isNaN(offset) ? 0 : offset;
    // Same.
    total = parseInt(total);
    total = isNaN(total) ? 0 : total;

    // We'll need to original total size as part of the Content-Range header
    // value in our response.
    var originalSize = size;

    // If we have a total size requested, we must make sure to send that number
    // of bytes only (minus the start offset).
    if (total && total < size) {
      size = total - offset;
    } else if (offset) {
      // Looks like we just have a byte offset to deal with.
      size = size - offset;
    }

    // We specifically need to add a Content-Range header to all responses for
    // requests that include a range request header.
    contentRange = "bytes " + offset + "-" + (size - 1) + "/" + originalSize;
  }

  // optional rate (in bytes/s) at which to send the file.
  if ("rate" in query) {
    rate = parseInt(query["rate"]);
  }

  // The context for the responseHandler.
  var context = {
    response: response,
    state: {
      contentType: contentType,
      totalBytes: size,
      sentBytes: 0,
      rate: rate
    }
  };

  // The notify implementation for the timer.
  context.notify = handleResponse.bind(context);

  timer =
    Components.classes["@mozilla.org/timer;1"]
              .createInstance(Components.interfaces.nsITimer);

  // sending at a specific rate requires our response to be asynchronous so
  // we handle all requests asynchronously. See handleResponse().
  response.processAsync();

  // generate the content.
  response.setStatusLine(version, statusCode, description);
  response.setHeader("Content-Type", contentType, false);
  if (contentRange) {
    response.setHeader("Content-Range", contentRange, false);
  }
  response.setHeader("Content-Length", size.toString(), false);

  // initialize the timer and start writing out the response.
  timer.initWithCallback(context,
                         1000,
                         Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);

}
