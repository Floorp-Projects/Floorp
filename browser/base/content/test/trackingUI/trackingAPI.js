function createIframe(src) {
  let ifr = document.createElement("iframe");
  ifr.src = src;
  document.body.appendChild(ifr);
}

onmessage = event => {
  switch (event.data) {
    case "tracking":
      createIframe("https://trackertest.org/");
      break;
    case "cryptomining":
      createIframe("http://cryptomining.example.com/");
      break;
    case "fingerprinting":
      createIframe("https://fingerprinting.example.com/");
      break;
    case "more-tracking":
      createIframe("https://itisatracker.org/");
      break;
    case "cookie":
      createIframe(
        "https://trackertest.org/browser/browser/base/content/test/trackingUI/cookieServer.sjs"
      );
      break;
    case "first-party-cookie":
      // Since the content blocking log doesn't seem to get updated for
      // top-level cookies right now, we just create an iframe with the
      // first party domain...
      createIframe(
        "http://not-tracking.example.com/browser/browser/base/content/test/trackingUI/cookieServer.sjs"
      );
      break;
    case "third-party-cookie":
      createIframe(
        "https://test1.example.org/browser/browser/base/content/test/trackingUI/cookieServer.sjs"
      );
      break;
    case "window-open":
      window.win = window.open(
        "http://trackertest.org/browser/browser/base/content/test/trackingUI/cookieServer.sjs",
        "_blank",
        "width=100,height=100"
      );
      break;
    case "window-close":
      window.win.close();
      window.win = null;
      break;
  }
};
