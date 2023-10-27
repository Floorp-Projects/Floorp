// eslint-disable-next-line complexity
onfetch = function (ev) {
  if (ev.request.url.includes("ignore")) {
    return;
  }

  if (ev.request.url.includes("bare-synthesized.txt")) {
    ev.respondWith(
      Promise.resolve(new Response("synthesized response body", {}))
    );
  } else if (ev.request.url.includes("file_CrossSiteXHR_server.sjs")) {
    // N.B. this response would break the rules of CORS if it were allowed, but
    //      this test relies upon the preflight request not being intercepted and
    //      thus this response should not be used.
    if (ev.request.method == "OPTIONS") {
      ev.respondWith(
        new Response("", {
          headers: {
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Headers": "X-Unsafe",
          },
        })
      );
    } else if (ev.request.url.includes("example.org")) {
      ev.respondWith(fetch(ev.request));
    }
  } else if (ev.request.url.includes("synthesized-404.txt")) {
    ev.respondWith(
      Promise.resolve(
        new Response("synthesized response body", { status: 404 })
      )
    );
  } else if (ev.request.url.includes("synthesized-headers.txt")) {
    ev.respondWith(
      Promise.resolve(
        new Response("synthesized response body", {
          headers: {
            "X-Custom-Greeting": "Hello",
          },
        })
      )
    );
  } else if (ev.request.url.includes("test-respondwith-response.txt")) {
    ev.respondWith(new Response("test-respondwith-response response body", {}));
  } else if (ev.request.url.includes("synthesized-redirect-real-file.txt")) {
    ev.respondWith(Promise.resolve(Response.redirect("fetch/real-file.txt")));
  } else if (
    ev.request.url.includes("synthesized-redirect-twice-real-file.txt")
  ) {
    ev.respondWith(
      Promise.resolve(Response.redirect("synthesized-redirect-real-file.txt"))
    );
  } else if (ev.request.url.includes("synthesized-redirect-synthesized.txt")) {
    ev.respondWith(Promise.resolve(Response.redirect("bare-synthesized.txt")));
  } else if (
    ev.request.url.includes("synthesized-redirect-twice-synthesized.txt")
  ) {
    ev.respondWith(
      Promise.resolve(Response.redirect("synthesized-redirect-synthesized.txt"))
    );
  } else if (ev.request.url.includes("rejected.txt")) {
    ev.respondWith(Promise.reject());
  } else if (ev.request.url.includes("nonresponse.txt")) {
    ev.respondWith(Promise.resolve(5));
  } else if (ev.request.url.includes("nonresponse2.txt")) {
    ev.respondWith(Promise.resolve({}));
  } else if (ev.request.url.includes("nonpromise.txt")) {
    try {
      // This should coerce to Promise(5) instead of throwing
      ev.respondWith(5);
    } catch (e) {
      // test is expecting failure, so return a success if we get a thrown
      // exception
      ev.respondWith(new Response("respondWith(5) threw " + e));
    }
  } else if (ev.request.url.includes("headers.txt")) {
    var ok = true;
    ok &= ev.request.headers.get("X-Test1") == "header1";
    ok &= ev.request.headers.get("X-Test2") == "header2";
    ev.respondWith(Promise.resolve(new Response(ok.toString(), {})));
  } else if (ev.request.url.includes("readable-stream.txt")) {
    ev.respondWith(
      new Response(
        new ReadableStream({
          start(controller) {
            controller.enqueue(
              new Uint8Array([0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21])
            );
            controller.close();
          },
        })
      )
    );
  } else if (ev.request.url.includes("readable-stream-locked.txt")) {
    let stream = new ReadableStream({
      start(controller) {
        controller.enqueue(
          new Uint8Array([0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21])
        );
        controller.close();
      },
    });

    ev.respondWith(new Response(stream));

    // This locks the stream.
    stream.getReader();
  } else if (ev.request.url.includes("readable-stream-with-exception.txt")) {
    ev.respondWith(
      new Response(
        new ReadableStream({
          start(controller) {},
          pull() {
            throw "EXCEPTION!";
          },
        })
      )
    );
  } else if (ev.request.url.includes("readable-stream-with-exception2.txt")) {
    ev.respondWith(
      new Response(
        new ReadableStream({
          _controller: null,
          _count: 0,

          start(controller) {
            this._controller = controller;
          },
          pull() {
            if (++this._count == 5) {
              throw "EXCEPTION 2!";
            }
            this._controller.enqueue(new Uint8Array([this._count]));
          },
        })
      )
    );
  } else if (ev.request.url.includes("readable-stream-already-consumed.txt")) {
    let r = new Response(
      new ReadableStream({
        start(controller) {
          controller.enqueue(
            new Uint8Array([0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21])
          );
          controller.close();
        },
      })
    );

    r.blob();

    ev.respondWith(r);
  } else if (ev.request.url.includes("user-pass")) {
    ev.respondWith(new Response(ev.request.url));
  } else if (ev.request.url.includes("nonexistent_image.gif")) {
    var imageAsBinaryString = atob(
      "R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs"
    );
    var imageLength = imageAsBinaryString.length;

    // If we just pass |imageAsBinaryString| to the Response constructor, an
    // encoding conversion occurs that corrupts the image. Instead, we need to
    // convert it to a typed array.
    // typed array.
    var imageAsArray = new Uint8Array(imageLength);
    for (var i = 0; i < imageLength; ++i) {
      imageAsArray[i] = imageAsBinaryString.charCodeAt(i);
    }

    ev.respondWith(
      Promise.resolve(
        new Response(imageAsArray, { headers: { "Content-Type": "image/gif" } })
      )
    );
  } else if (ev.request.url.includes("nonexistent_script.js")) {
    ev.respondWith(
      Promise.resolve(new Response("check_intercepted_script();", {}))
    );
  } else if (ev.request.url.includes("nonexistent_stylesheet.css")) {
    ev.respondWith(
      Promise.resolve(
        new Response("#style-test { background-color: black !important; }", {
          headers: {
            "Content-Type": "text/css",
          },
        })
      )
    );
  } else if (ev.request.url.includes("nonexistent_page.html")) {
    ev.respondWith(
      Promise.resolve(
        new Response(
          "<script>window.frameElement.test_result = true;</script>",
          {
            headers: {
              "Content-Type": "text/html",
            },
          }
        )
      )
    );
  } else if (ev.request.url.includes("navigate.html")) {
    var requests = [
      // should not throw
      new Request(ev.request),
      new Request(ev.request, undefined),
      new Request(ev.request, null),
      new Request(ev.request, {}),
      new Request(ev.request, { someUnrelatedProperty: 42 }),
      new Request(ev.request, { method: "GET" }),
    ];
    ev.respondWith(
      Promise.resolve(
        new Response(
          "<script>window.frameElement.test_result = true;</script>",
          {
            headers: {
              "Content-Type": "text/html",
            },
          }
        )
      )
    );
  } else if (ev.request.url.includes("nonexistent_worker_script.js")) {
    ev.respondWith(
      Promise.resolve(
        new Response("postMessage('worker-intercept-success')", {
          headers: { "Content-Type": "text/javascript" },
        })
      )
    );
  } else if (ev.request.url.includes("nonexistent_imported_script.js")) {
    ev.respondWith(
      Promise.resolve(
        new Response("check_intercepted_script();", {
          headers: { "Content-Type": "text/javascript" },
        })
      )
    );
  } else if (ev.request.url.includes("deliver-gzip")) {
    // Don't handle the request, this will make Necko perform a network request, at
    // which point SetApplyConversion must be re-enabled, otherwise the request
    // will fail.
    // eslint-disable-next-line no-useless-return
    return;
  } else if (ev.request.url.includes("hello.gz")) {
    ev.respondWith(fetch("fetch/deliver-gzip.sjs"));
  } else if (ev.request.url.includes("hello-after-extracting.gz")) {
    ev.respondWith(
      fetch("fetch/deliver-gzip.sjs").then(function (res) {
        return res.text().then(function (body) {
          return new Response(body, {
            status: res.status,
            statusText: res.statusText,
            headers: res.headers,
          });
        });
      })
    );
  } else if (ev.request.url.includes("opaque-on-same-origin")) {
    var url =
      "http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200";
    ev.respondWith(fetch(url, { mode: "no-cors" }));
  } else if (ev.request.url.includes("opaque-no-cors")) {
    if (ev.request.mode != "no-cors") {
      ev.respondWith(Promise.reject());
      return;
    }

    var url =
      "http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200";
    ev.respondWith(fetch(url, { mode: ev.request.mode }));
  } else if (ev.request.url.includes("cors-for-no-cors")) {
    if (ev.request.mode != "no-cors") {
      ev.respondWith(Promise.reject());
      return;
    }

    var url =
      "http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*";
    ev.respondWith(fetch(url));
  } else if (ev.request.url.includes("example.com")) {
    ev.respondWith(fetch(ev.request));
  } else if (ev.request.url.includes("body-")) {
    ev.respondWith(
      ev.request.text().then(function (body) {
        return new Response(body + body);
      })
    );
  } else if (ev.request.url.includes("something.txt")) {
    ev.respondWith(Response.redirect("fetch/somethingelse.txt"));
  } else if (ev.request.url.includes("somethingelse.txt")) {
    ev.respondWith(new Response("something else response body", {}));
  } else if (ev.request.url.includes("redirect_serviceworker.sjs")) {
    // The redirect_serviceworker.sjs server-side JavaScript file redirects to
    // 'http://mochi.test:8888/tests/dom/serviceworkers/test/worker.js'
    // The redirected fetch should not go through the SW since the original
    // fetch was initiated from a SW.
    ev.respondWith(fetch("redirect_serviceworker.sjs"));
  } else if (
    ev.request.url.includes("load_cross_origin_xml_document_synthetic.xml")
  ) {
    ev.respondWith(
      Promise.resolve(
        new Response("<response>body</response>", {
          headers: { "Content-Type": "text/xtml" },
        })
      )
    );
  } else if (
    ev.request.url.includes("load_cross_origin_xml_document_cors.xml")
  ) {
    if (ev.request.mode != "same-origin") {
      ev.respondWith(Promise.reject());
      return;
    }

    var url =
      "http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*";
    ev.respondWith(fetch(url, { mode: "cors" }));
  } else if (
    ev.request.url.includes("load_cross_origin_xml_document_opaque.xml")
  ) {
    if (ev.request.mode != "same-origin") {
      Promise.resolve(
        new Response("<error>Invalid Request mode</error>", {
          headers: { "Content-Type": "text/xtml" },
        })
      );
      return;
    }

    var url =
      "http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200";
    ev.respondWith(fetch(url, { mode: "no-cors" }));
  } else if (ev.request.url.includes("xhr-method-test.txt")) {
    ev.respondWith(new Response("intercepted " + ev.request.method));
  } else if (ev.request.url.includes("empty-header")) {
    if (
      !ev.request.headers.has("emptyheader") ||
      ev.request.headers.get("emptyheader") !== ""
    ) {
      ev.respondWith(Promise.reject());
      return;
    }
    ev.respondWith(new Response("emptyheader"));
  } else if (ev.request.url.includes("fetchevent-extendable")) {
    if (ev instanceof ExtendableEvent) {
      ev.respondWith(new Response("extendable"));
    } else {
      ev.respondWith(Promise.reject());
    }
  } else if (ev.request.url.includes("fetchevent-request")) {
    var threw = false;
    try {
      new FetchEvent("foo");
    } catch (e) {
      if (e.name == "TypeError") {
        threw = true;
      }
    } finally {
      ev.respondWith(new Response(threw ? "non-nullable" : "nullable"));
    }
  }
};
