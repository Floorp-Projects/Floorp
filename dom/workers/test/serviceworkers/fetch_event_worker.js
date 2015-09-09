var seenIndex = false;

onfetch = function(ev) {
  if (ev.request.url.includes("bare-synthesized.txt")) {
    ev.respondWith(Promise.resolve(
      new Response("synthesized response body", {})
    ));
  }

  else if (ev.request.url.includes('file_CrossSiteXHR_server.sjs')) {
    // N.B. this response would break the rules of CORS if it were allowed, but
    //      this test relies upon the preflight request not being intercepted and
    //      thus this response should not be used.
    if (ev.request.method == 'OPTIONS') {
      ev.respondWith(new Response('', {headers: {'Access-Control-Allow-Origin': '*',
                                                 'Access-Control-Allow-Headers': 'X-Unsafe'}}))
    } else if (ev.request.url.includes('example.org')) {
      ev.respondWith(fetch(ev.request));
    }
  }

  else if (ev.request.url.includes("synthesized-404.txt")) {
    ev.respondWith(Promise.resolve(
      new Response("synthesized response body", { status: 404 })
    ));
  }

  else if (ev.request.url.includes("synthesized-headers.txt")) {
    ev.respondWith(Promise.resolve(
      new Response("synthesized response body", {
        headers: {
          "X-Custom-Greeting": "Hello"
        }
      })
    ));
  }

  else if (ev.request.url.includes("test-respondwith-response.txt")) {
    ev.respondWith(new Response("test-respondwith-response response body", {}));
  }

  else if (ev.request.url.includes("synthesized-redirect-real-file.txt")) {
    ev.respondWith(Promise.resolve(
      Response.redirect("fetch/real-file.txt")
    ));
  }

  else if (ev.request.url.includes("synthesized-redirect-twice-real-file.txt")) {
    ev.respondWith(Promise.resolve(
      Response.redirect("synthesized-redirect-real-file.txt")
    ));
  }

  else if (ev.request.url.includes("synthesized-redirect-synthesized.txt")) {
    ev.respondWith(Promise.resolve(
      Response.redirect("bare-synthesized.txt")
    ));
  }

  else if (ev.request.url.includes("synthesized-redirect-twice-synthesized.txt")) {
    ev.respondWith(Promise.resolve(
      Response.redirect("synthesized-redirect-synthesized.txt")
     ));
   }

  else if (ev.request.url.includes("ignored.txt")) {
  }

  else if (ev.request.url.includes("rejected.txt")) {
    ev.respondWith(Promise.reject());
  }

  else if (ev.request.url.includes("nonresponse.txt")) {
    ev.respondWith(Promise.resolve(5));
  }

  else if (ev.request.url.includes("nonresponse2.txt")) {
    ev.respondWith(Promise.resolve({}));
  }

  else if (ev.request.url.includes("nonpromise.txt")) {
    try {
      // This should coerce to Promise(5) instead of throwing
      ev.respondWith(5);
    } catch (e) {
      // test is expecting failure, so return a success if we get a thrown
      // exception
      ev.respondWith(new Response('respondWith(5) threw ' + e));
    }
  }

  else if (ev.request.url.includes("headers.txt")) {
    var ok = true;
    ok &= ev.request.headers.get("X-Test1") == "header1";
    ok &= ev.request.headers.get("X-Test2") == "header2";
    ev.respondWith(Promise.resolve(
      new Response(ok.toString(), {})
    ));
  }

  else if (ev.request.url.includes("nonexistent_image.gif")) {
    var imageAsBinaryString = atob("R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs");
    var imageLength = imageAsBinaryString.length;

    // If we just pass |imageAsBinaryString| to the Response constructor, an
    // encoding conversion occurs that corrupts the image. Instead, we need to
    // convert it to a typed array.
    // typed array.
    var imageAsArray = new Uint8Array(imageLength);
    for (var i = 0; i < imageLength; ++i) {
      imageAsArray[i] = imageAsBinaryString.charCodeAt(i);
    }

    ev.respondWith(Promise.resolve(
      new Response(imageAsArray, { headers: { "Content-Type": "image/gif" } })
    ));
  }

  else if (ev.request.url.includes("nonexistent_script.js")) {
    ev.respondWith(Promise.resolve(
      new Response("check_intercepted_script();", {})
    ));
  }

  else if (ev.request.url.includes("nonexistent_stylesheet.css")) {
    ev.respondWith(Promise.resolve(
      new Response("#style-test { background-color: black !important; }", {
        headers : {
          "Content-Type": "text/css"
        }
      })
    ));
  }

  else if (ev.request.url.includes("nonexistent_page.html")) {
    ev.respondWith(Promise.resolve(
      new Response("<script>window.frameElement.test_result = true;</script>", {
        headers : {
          "Content-Type": "text/html"
        }
      })
    ));
  }

  else if (ev.request.url.includes("nonexistent_worker_script.js")) {
    ev.respondWith(Promise.resolve(
      new Response("postMessage('worker-intercept-success')", {})
    ));
  }

  else if (ev.request.url.includes("nonexistent_imported_script.js")) {
    ev.respondWith(Promise.resolve(
      new Response("check_intercepted_script();", {})
    ));
  }

  else if (ev.request.url.includes("deliver-gzip")) {
    // Don't handle the request, this will make Necko perform a network request, at
    // which point SetApplyConversion must be re-enabled, otherwise the request
    // will fail.
    return;
  }

  else if (ev.request.url.includes("hello.gz")) {
    ev.respondWith(fetch("fetch/deliver-gzip.sjs"));
  }

  else if (ev.request.url.includes("hello-after-extracting.gz")) {
    ev.respondWith(fetch("fetch/deliver-gzip.sjs").then(function(res) {
      return res.text().then(function(body) {
        return new Response(body, { status: res.status, statusText: res.statusText, headers: res.headers });
      });
    }));
  }

  else if (ev.request.url.includes('opaque-on-same-origin')) {
    var url = 'http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200';
    ev.respondWith(fetch(url, { mode: 'no-cors' }));
  }

  else if (ev.request.url.includes('opaque-no-cors')) {
    if (ev.request.mode != "no-cors") {
      ev.respondWith(Promise.reject());
      return;
    }

    var url = 'http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200';
    ev.respondWith(fetch(url, { mode: ev.request.mode }));
  }

  else if (ev.request.url.includes('cors-for-no-cors')) {
    if (ev.request.mode != "no-cors") {
      ev.respondWith(Promise.reject());
      return;
    }

    var url = 'http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*';
    ev.respondWith(fetch(url));
  }

  else if (ev.request.url.includes('example.com')) {
    ev.respondWith(fetch(ev.request));
  }

  else if (ev.request.url.includes("index.html")) {
    if (seenIndex) {
        var body = "<script>" +
                     "opener.postMessage({status: 'ok', result: " + ev.isReload + "," +
                                         "message: 'reload status should be indicated'}, '*');" +
                     "opener.postMessage({status: 'done'}, '*');" +
                   "</script>";
        ev.respondWith(new Response(body, {headers: {'Content-Type': 'text/html'}}));
    } else {
      seenIndex = true;
      ev.respondWith(fetch(ev.request.url));
    }
  }

  else if (ev.request.url.includes("body-")) {
    ev.respondWith(ev.request.text().then(function (body) {
      return new Response(body + body);
    }));
  }

  else if (ev.request.url.includes('something.txt')) {
    ev.respondWith(Response.redirect('fetch/somethingelse.txt'));
  }

  else if (ev.request.url.includes('somethingelse.txt')) {
    ev.respondWith(new Response('something else response body', {}));
  }

  else if (ev.request.url.includes('redirect_serviceworker.sjs')) {
    // The redirect_serviceworker.sjs server-side JavaScript file redirects to
    // 'http://mochi.test:8888/tests/dom/workers/test/serviceworkers/worker.js'
    // The redirected fetch should not go through the SW since the original
    // fetch was initiated from a SW.
    ev.respondWith(fetch('redirect_serviceworker.sjs'));
  }

  else if (ev.request.url.includes('load_cross_origin_xml_document_synthetic.xml')) {
    if (ev.request.mode != 'same-origin') {
      ev.respondWith(Promise.reject());
      return;
    }

    ev.respondWith(Promise.resolve(
      new Response("<response>body</response>", { headers: {'Content-Type': 'text/xtml'}})
    ));
  }

  else if (ev.request.url.includes('load_cross_origin_xml_document_cors.xml')) {
    if (ev.request.mode != 'same-origin') {
      ev.respondWith(Promise.reject());
      return;
    }

    var url = 'http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*';
    ev.respondWith(fetch(url, { mode: 'cors' }));
  }

  else if (ev.request.url.includes('load_cross_origin_xml_document_opaque.xml')) {
    if (ev.request.mode != 'same-origin') {
      Promise.resolve(
        new Response("<error>Invalid Request mode</error>", { headers: {'Content-Type': 'text/xtml'}})
      );
      return;
    }

    var url = 'http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200';
    ev.respondWith(fetch(url, { mode: 'no-cors' }));
  }

  else if (ev.request.url.includes('xhr-method-test.txt')) {
    ev.respondWith(new Response('intercepted ' + ev.request.method));
  }
};
