/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onmessage = function(event) {
  if (event.data.data == 0) {
    ab = new ArrayBuffer(event.data.size);
    postMessage({ event: "W: The size is: " + event.data.size + " == " + ab.byteLength,
                  status: ab.byteLength == event.data.size, last: false });

    postMessage({ event: "W: postMessage with arrayBuffer", status: true,
                  ab: ab, bc: [ ab, ab, { dd: ab } ] }, [ab]);

    postMessage({ event: "W: The size is: 0 == " + ab.byteLength,
                  status: ab.byteLength == 0, last: false });

    postMessage({ event: "last one!", status: true, last: true });

  } else {
    var worker = new Worker('sync_worker.js');
    worker.onmessage = function(event) {
      postMessage(event.data);
    }
    worker.onsyncmessage = function(event) {
      var v = postSyncMessage(event.data, null, 500);
      event.reply(v);
    }

    --event.data.data;
    worker.postMessage(event.data);
  }
}
