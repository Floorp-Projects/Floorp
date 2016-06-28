onmessage = function(e) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', e.data, true);
  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
      postMessage(xhr.status);
    }
  };
  xhr.send();
};
