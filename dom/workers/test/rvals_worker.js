onmessage = function(evt) {
  postMessage(postMessage('ignore') == undefined);

  var id = setInterval(function() {}, 200);
  postMessage(clearInterval(id) == undefined);

  id = setTimeout(function() {}, 200);
  postMessage(clearTimeout(id) == undefined);

  postMessage(dump(42 + '\n') == undefined);

  postMessage('finished');
}
