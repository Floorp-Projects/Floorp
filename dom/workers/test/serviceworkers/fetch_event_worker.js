onfetch = function(ev) {
  if (ev.request.url.contains("synthesized.txt")) {
    var p = new Promise(function(resolve) {
      var r = new Response("synthesized response body", {});
      resolve(r);
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("ignored.txt")) {
  }

  else if (ev.request.url.contains("rejected.txt")) {
    var p = new Promise(function(resolve, reject) {
      reject();
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("nonresponse.txt")) {
    var p = new Promise(function(resolve, reject) {
      resolve(5);
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("nonresponse2.txt")) {
    var p = new Promise(function(resolve, reject) {
      resolve({});
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("headers.txt")) {
    var p = new Promise(function(resolve, reject) {
      var ok = true;
      ok &= ev.request.headers.get("X-Test1") == "header1";
      ok &= ev.request.headers.get("X-Test2") == "header2";
      var r = new Response(ok.toString(), {});
      resolve(r);
    });
    ev.respondWith(p);    
  }

  else if (ev.request.url.contains("nonexistent_image.gif")) {
    var p = new Promise(function(resolve, reject) {
      resolve(new Response(atob("R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs"), {}));
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("nonexistent_script.js")) {
    var p = new Promise(function(resolve, reject) {
      resolve(new Response("check_intercepted_script();", {}));
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("nonexistent_stylesheet.css")) {
    var p = new Promise(function(resolve, reject) {
      resolve(new Response("#style-test { background-color: black !important; }", {}));
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("nonexistent_page.html")) {
    var p = new Promise(function(resolve, reject) {
      resolve(new Response("<script>window.frameElement.test_result = true;</script>", {}));
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("nonexistent_worker_script.js")) {
    var p = new Promise(function(resolve, reject) {
      resolve(new Response("postMessage('worker-intercept-success')", {}));
    });
    ev.respondWith(p);
  }

  else if (ev.request.url.contains("nonexistent_imported_script.js")) {
    var p = new Promise(function(resolve, reject) {
      resolve(new Response("check_intercepted_script();", {}));
    });
    ev.respondWith(p);
  }
}
