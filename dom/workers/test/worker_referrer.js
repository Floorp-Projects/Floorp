onmessage = function() {
  importScripts(['referrer.sjs?import']);
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'referrer.sjs?result', true);
  xhr.onload = function() {
    postMessage(xhr.responseText);
  }
  xhr.send();
}
