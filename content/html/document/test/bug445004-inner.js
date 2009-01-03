document.domain = "example.org";
function $(str) { return document.getElementById(str); }
function hookLoad(str) {
  $(str).onload = function() { window.parent.parent.postMessage('end', '*'); };
  window.parent.parent.postMessage('start', '*');
}
window.onload = function() {
  hookLoad("w");
  $("w").contentWindow.location.href = "test1.example.org.png";
  hookLoad("x");
  var doc = $("x").contentDocument;
  doc.write('<img src="test1.example.org.png">');
  doc.close();
};
function doIt() {
  hookLoad("y");
  $("y").contentWindow.location.href = "example.org.png";
  hookLoad("z");
  var doc = $("z").contentDocument;
  doc.write('<img src="example.org.png">');
  doc.close();
}
window.addEventListener("message", doIt, false);