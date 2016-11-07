function loadTest(file) {
  var iframe = document.createElement('iframe');
  iframe.src = file;

  document.body.appendChild(iframe);
}

function setupTest() {
  window.SimpleTest = parent.SimpleTest;
  window.is = parent.is;
  window.isnot = parent.isnot;
  window.ok = parent.ok;
  window.info = parent.info;
}
