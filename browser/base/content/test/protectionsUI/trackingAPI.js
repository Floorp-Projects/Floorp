function createIframe(src) {
  let ifr = document.createElement("iframe");
  ifr.src = src;
  document.body.appendChild(ifr);
}

function createImage(src) {
  let img = document.createElement("img");
  img.src = src;
  img.onload = () => {
    parent.postMessage("done", "*");
  };
  document.body.appendChild(img);
}

onmessage = event => {
  switch (event.data) {
    case "tracking":
      createIframe("https://trackertest.org/");
      break;
    case "socialtracking":
      createIframe(
        "https://social-tracking.example.org/browser/browser/base/content/test/protectionsUI/cookieServer.sjs"
      );
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
    case "more-tracking-2":
      createIframe("https://tracking.example.com/");
      break;
    case "cookie":
      createIframe(
        "https://trackertest.org/browser/browser/base/content/test/protectionsUI/cookieServer.sjs"
      );
      break;
    case "first-party-cookie":
      // Since the content blocking log doesn't seem to get updated for
      // top-level cookies right now, we just create an iframe with the
      // first party domain...
      createIframe(
        "http://not-tracking.example.com/browser/browser/base/content/test/protectionsUI/cookieServer.sjs"
      );
      break;
    case "third-party-cookie":
      createIframe(
        "https://test1.example.org/browser/browser/base/content/test/protectionsUI/cookieServer.sjs"
      );
      break;
    case "image":
      createImage(
        "http://trackertest.org/browser/browser/base/content/test/protectionsUI/cookieServer.sjs?type=image-no-cookie"
      );
      break;
    case "window-open":
      window.win = window.open(
        "http://trackertest.org/browser/browser/base/content/test/protectionsUI/cookieServer.sjs",
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
