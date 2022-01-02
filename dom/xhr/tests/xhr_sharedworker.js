/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint-env worker */
onconnect = e => {
  e.ports[0].onmessage = event => {
    const url = event.data;

    var xhr = new XMLHttpRequest();
    xhr.open("GET", url, false);
    xhr.send();

    const refText = xhr.responseText;

    function getResponse(type) {
      xhr = new XMLHttpRequest();
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
      xhr = new XMLHttpRequest();
      xhr.open("GET", url, false);
      xhr.responseType = type;
      xhr.send();

      var exception;

      try {
        xhr.responseText;
      } catch (ex) {
        exception = ex;
      }

      if (!exception) {
        throw new Error(
          `Failed to throw when getting responseText on ${type} type`
        );
      }

      if (exception.name != "InvalidStateError") {
        throw new Error(
          `Unexpected error when getting responseText on ${type} type`
        );
      }

      if (exception.code != DOMException.INVALID_STATE_ERR) {
        throw new Error(
          `Unexpected error code when getting responseText on ${type} type`
        );
      }
    }

    testResponseTextException("arraybuffer");
    testResponseTextException("blob");

    // Make sure "document" works, but returns text.
    xhr = new XMLHttpRequest();

    if (xhr.responseType != "") {
      throw new Error("Default value for responseType is wrong!");
    }

    xhr.open("GET", url, false);
    xhr.responseType = "document";
    xhr.send();

    if (xhr.responseText != refText) {
      throw new Error("'document' type not working correctly");
    }

    e.ports[0].postMessage("done");
  };
};
