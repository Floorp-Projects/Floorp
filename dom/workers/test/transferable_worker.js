/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onmessage = function(event) {
  if ("notEmpty" in event.data && "byteLength" in event.data.notEmpty) {
    postMessage({ event: "W: NotEmpty object received: " + event.data.notEmpty.byteLength,
                  status: event.data.notEmpty.byteLength != 0, last: false });
  }

  var ab = new ArrayBuffer(event.data.size);
  postMessage({ event: "W: The size is: " + event.data.size + " == " + ab.byteLength,
                status: ab.byteLength == event.data.size, last: false });

  postMessage({ event: "W: postMessage with arrayBuffer", status: true,
                notEmpty: ab, ab: ab, bc: [ ab, ab, { dd: ab } ] }, [ab]);

  postMessage({ event: "W: The size is: 0 == " + ab.byteLength,
                status: ab.byteLength == 0, last: false });

  postMessage({ event: "W: last one!", status: true, last: true });
}
