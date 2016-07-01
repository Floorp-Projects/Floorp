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
    if (e.data.inherited == "parent") {
      //Worker inherits CSP from parent worker
      prefix = base + "worker_child_inherited_parent_";
      suffix = base == sameBase ? "_good" : "_bad";
    } else if (e.data.inherited == "document") {
      //Worker inherits CSP from owner document -> parent worker -> subworker
      prefix = base + "worker_child_inherited_document_";
      suffix = base == sameBase ? "_good" : "_bad";
    } else {
      // Worker delivers CSP from HTTP header
      prefix = base + "worker_child_";
      suffix = base == sameBase ? "_same_bad" : "_cross_bad";
    }

    doXHR(prefix + "xhr" + suffix);
    // Fetch is likely failed in subworker
    // See Bug 1273070 - Failed to fetch in subworker
    // Enable fetch test after the bug is fixed
    // fetch(prefix + "xhr" + suffix);
    try {
      importScripts(prefix + "script" + suffix);
    } catch(ex) {}
  }
}
