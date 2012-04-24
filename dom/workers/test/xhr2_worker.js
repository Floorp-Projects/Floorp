/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onmessage = function(event) {
  const url = event.data;

  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, false);
  xhr.send();

  const refText = xhr.responseText;

  function getResponse(type) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url, false);
    if (type !== undefined) {
      xhr.responseType = type;
    }
    xhr.send();
    return xhr.response;
  }

  if (getResponse() != refText) {
    throw new Error("unset responseType failed");
  }

  if (getResponse("") != refText) {
    throw new Error("'' responseType failed");
  }

  if (getResponse("text") != refText) {
    throw new Error("'text' responseType failed");
  }

  var array = new Uint8Array(getResponse("arraybuffer"));
  if (String.fromCharCode.apply(String, array) != refText) {
    throw new Error("'arraybuffer' responseType failed");
  }

  var blob = getResponse("blob");
  if (new FileReaderSync().readAsText(blob) != refText) {
    throw new Error("'blob' responseType failed");
  }

  // Make sure that we get invalid state exceptions when getting the wrong
  // property.

  function testResponseTextException(type) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url, false);
    xhr.responseType = type;
    xhr.send();

    var exception;

    try {
      xhr.responseText;
    }
    catch(e) {
      exception = e;
    }

    if (!exception) {
      throw new Error("Failed to throw when getting responseText on '" + type +
                      "' type");
    }

    if (exception.name != "InvalidStateError") {
      throw new Error("Unexpected error when getting responseText on '" + type +
                      "' type");
    }

    if (exception.code != DOMException.INVALID_STATE_ERR) {
      throw new Error("Unexpected error code when getting responseText on '" + type +
                      "' type");
    }
  }

  testResponseTextException("arraybuffer");
  testResponseTextException("blob");

  // Make sure "document" works, but returns text.
  xhr = new XMLHttpRequest();

  if (xhr.responseType != "text") {
    throw new Error("Default value for responseType is wrong!");
  }

  xhr.open("GET", url, false);
  xhr.responseType = "document";
  xhr.send();

  if (xhr.responseText != refText) {
    throw new Error("'document' type not working correctly");
  }

  // Make sure setting responseType before open or after send fails.
  var exception;

  xhr = new XMLHttpRequest();
  try {
    xhr.responseType = "arraybuffer";
  }
  catch(e) {
    exception = e;
  }

  if (!exception) {
    throw new Error("Failed to throw when setting responseType before " +
                    "calling open()");
  }

  if (exception.name != "InvalidStateError") {
    throw new Error("Unexpected error when setting responseType before " +
                    "calling open()");
  }

  if (exception.code != DOMException.INVALID_STATE_ERR) {
    throw new Error("Unexpected error code when setting responseType before " +
                    "calling open()");
  }

  xhr.open("GET", url);
  xhr.responseType = "text";
  xhr.onload = function(event) {
    if (event.target.response != refText) {
      throw new Error("Bad response!");
    }

    xhr = new XMLHttpRequest();
    xhr.open("GET", url);
    xhr.responseType = "moz-chunked-text";

    var lastIndex = 0;
    xhr.onprogress = function(event) {
      if (refText.substr(lastIndex, xhr.response.length) != xhr.response) {
        throw new Error("Bad chunk!");
      }

      lastIndex += xhr.response.length;
    };

    xhr.onload = function(event) {
      if (lastIndex != refText.length) {
        throw new Error("Didn't see all the data!");
      }

      setTimeout(function() {
        if (xhr.response !== null) {
          throw new Error("Should have gotten null response outside of event!");
        }
        postMessage("done");
      }, 0);
    }

    xhr.send(null);
  };
  xhr.send();

  exception = null;

  try {
    xhr.responseType = "arraybuffer";
  }
  catch(e) {
    exception = e;
  }

  if (!exception) {
    throw new Error("Failed to throw when setting responseType after " +
                    "calling send()");
  }

  if (exception.name != "InvalidStateError") {
    throw new Error("Unexpected error when setting responseType after " +
                    "calling send()");
  }

  if (exception.code != DOMException.INVALID_STATE_ERR) {
    throw new Error("Unexpected error code when setting responseType after " +
                    "calling send()");
  }
}
