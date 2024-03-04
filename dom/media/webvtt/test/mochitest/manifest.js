// Force releasing decoder to avoid timeout in waiting for decoding resource.
function removeNodeAndSource(n) {
  n.remove();
  // reset |srcObject| first since it takes precedence over |src|.
  n.srcObject = null;
  n.removeAttribute("src");
  n.load();
  while (n.firstChild) {
    n.firstChild.remove();
  }
}

function once(target, name, cb) {
  var p = new Promise(function (resolve) {
    target.addEventListener(
      name,
      function () {
        resolve();
      },
      { once: true }
    );
  });
  if (cb) {
    p.then(cb);
  }
  return p;
}
