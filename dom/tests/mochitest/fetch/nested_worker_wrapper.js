addEventListener('message', function nestedWorkerWrapperOnMessage(evt) {
  removeEventListener('message', nestedWorkerWrapperOnMessage);

  var worker = new Worker('worker_wrapper.js');

  worker.addEventListener('message', function(evt) {
    self.postMessage({
      context: 'NestedWorker',
      type: evt.data.type,
      status: evt.data.status,
      msg: evt.data.msg,
    });
  });

  worker.addEventListener('error', function(evt) {
    self.postMessage({
      context: 'NestedWorker',
      type: 'status',
      status: false,
      msg: 'Nested worker error: ' + evt.message,
    });
  });

  worker.postMessage(evt.data);
});
