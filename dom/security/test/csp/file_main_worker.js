function doXHR(uri) {
  try {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", uri);
    xhr.send();
  } catch(ex) {}
}

var sameBase = "http://mochi.test:8888/tests/dom/security/test/csp/file_CSP.sjs?testid=";
var crossBase = "http://example.com/tests/dom/security/test/csp/file_CSP.sjs?testid=";

onmessage = (e) => {
  for (base of [sameBase, crossBase]) {
    var prefix;
    var suffix;
    if (e.data.inherited) {
      prefix = base + "worker_inherited_"
      suffix = base == sameBase ? "_good" : "_bad";
    }
    else {
      prefix = base + "worker_"
      suffix = base == sameBase ? "_same_good" : "_cross_good";
    }
    doXHR(prefix + "xhr" + suffix);
    fetch(prefix + "fetch" + suffix);
    try { importScripts(prefix + "script" + suffix); } catch(ex) {}
  }
}
