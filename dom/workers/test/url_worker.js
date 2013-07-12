onmessage = function(event) {
  if (event.data != 0) {
    var worker = new Worker('url_worker.js');
    worker.onmessage = function(event) {
      postMessage(event.data);
    }

    worker.postMessage(event.data - 1);
    return;
  }

  status = false;
  try {
    if ((URL instanceof Object)) {
      status = true;
    }
  } catch(e) {
  }

  postMessage({type: 'status', status: status, msg: 'URL object:' + URL});

  status = false;
  var blob = null;
  try {
    blob = new Blob([]);
    status = true;
  } catch(e) {
  }

  postMessage({type: 'status', status: status, msg: 'Blob:' + blob});

  status = false;
  var url = null;
  try {
    url = URL.createObjectURL(blob);
    status = true;
  } catch(e) {
  }

  postMessage({type: 'status', status: status, msg: 'Blob URL:' + url});

  status = false;
  try {
    URL.revokeObjectURL(url);
    status = true;
  } catch(e) {
  }

  postMessage({type: 'status', status: status, msg: 'Blob Revoke URL'});

  status = false;
  var url = null;
  try {
    url = URL.createObjectURL(true);
  } catch(e) {
    status = true;
  }

  postMessage({type: 'status', status: status, msg: 'CreateObjectURL should fail if the arg is not a blob'});

  status = false;
  var url = null;
  try {
    url = URL.createObjectURL(blob);
    status = true;
  } catch(e) {
  }

  postMessage({type: 'status', status: status, msg: 'Blob URL2:' + url});
  postMessage({type: 'url', url: url});

  status = false;
  try {
    URL.createObjectURL(new Object());
  } catch(e) {
    status = true;
  }

  postMessage({type: 'status', status: status, msg: 'Exception wanted' });

  postMessage({type: 'finish' });
}
