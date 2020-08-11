class A {
  #x;
}

let objects = [];

self.onmessage = function(e) {
  if (e.data === 'allocate') {
    objects.push(new A);
    return;
  }
  if (e.data == 'count') {
    postMessage(objects.length);
    return;
  }
  postMessage('Unknown message type.');
}