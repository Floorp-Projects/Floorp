class A {
  static { this.x = 12; }
}


self.onmessage = function (e) {
  console.log(e)
  if (e.data == 'get') {
    postMessage(A.x);
    return;
  }
  postMessage('Unknown message type.');
}