var xhr = new XMLHttpRequest();
xhr.responseType = 'blob';
xhr.open('GET', 'worker_bug1278777.js');

xhr.onload = function() {
  postMessage(xhr.response instanceof Blob);
}

xhr.send();
