onmessage = function(evt) {
  if (evt.data != 0) {
    var worker = new Worker('jsversion_worker.js');
    worker.onmessage = function(evt) {
      postMessage(evt.data);
    }

    worker.postMessage(evt.data - 1);
    return;
  }

  let foo = 'bar';
  postMessage(true);
}
