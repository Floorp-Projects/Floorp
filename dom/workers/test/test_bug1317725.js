onmessage = function (e) {
  var data = new FormData();
  data.append("Filedata", e.data.slice(0, 127), encodeURI(e.data.name));
  var xhr = new XMLHttpRequest();
  xhr.open("POST", location.href, false);
  xhr.send(data);
  postMessage("No crash \\o/");
};
