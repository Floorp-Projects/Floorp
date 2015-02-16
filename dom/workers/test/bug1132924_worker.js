onmessage = function() {
  var a = new XMLHttpRequest();
  a.open('GET', 'empty.html', false);
  a.onreadystatechange = function() {
    if (a.readyState == 4) {
      postMessage(a.response);
    }
  }
  a.send(null);
}
