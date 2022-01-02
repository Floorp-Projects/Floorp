onfetch = function(e) {
  if (e.request.url.includes("Referer")) {
    // Silently rewrite the referrer so the referrer test passes since the
    // document/worker isn't aware of this service worker.
    var url = e.request.url.substring(0, e.request.url.indexOf("?"));
    url += "?headers=" + JSON.stringify({ Referer: self.location.href });

    e.respondWith(
      e.request.text().then(function(text) {
        var body = text === "" ? undefined : text;
        var mode =
          e.request.mode == "navigate" ? "same-origin" : e.request.mode;
        return fetch(url, {
          method: e.request.method,
          headers: e.request.headers,
          body,
          mode,
          credentials: e.request.credentials,
          redirect: e.request.redirect,
          cache: e.request.cache,
        });
      })
    );
    return;
  }
  e.respondWith(fetch(e.request));
};
