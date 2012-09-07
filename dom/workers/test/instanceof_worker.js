/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onmessage = function(event) {
  postMessage({event: "XMLHttpRequest",
               status: (new XMLHttpRequest() instanceof XMLHttpRequest),
               last: false });
  postMessage({event: "XMLHttpRequestUpload",
               status: ((new XMLHttpRequest()).upload instanceof XMLHttpRequestUpload),
               last: true });
}
