onmessage = event => {
  switch (event.data) {
  case "tracking": {
      let ifr = document.createElement("iframe");
      ifr.src = "https://trackertest.org/";
      document.body.appendChild(ifr);
    }
    break;
  case "more-tracking": {
      let ifr = document.createElement("iframe");
      ifr.src = "https://itisatracker.org/";
      document.body.appendChild(ifr);
    }
    break;
  case "cookie": {
      let ifr = document.createElement("iframe");
      ifr.src = "https://trackertest.org/browser/browser/base/content/test/trackingUI/cookieServer.sjs";
      document.body.appendChild(ifr);
    }
    break;
  case "first-party-cookie": {
      // Since the content blocking log doesn't seem to get updated for
      // top-level cookies right now, we just create an iframe with the
      // first party domain...
      let ifr = document.createElement("iframe");
      ifr.src = "http://not-tracking.example.com/browser/browser/base/content/test/trackingUI/cookieServer.sjs";
      document.body.appendChild(ifr);
    }
    break;
  case "third-party-cookie": {
      let ifr = document.createElement("iframe");
      ifr.src = "https://test1.example.org/browser/browser/base/content/test/trackingUI/cookieServer.sjs";
      document.body.appendChild(ifr);
    }
    break;
  case "window-open":
      window.win = window.open("http://trackertest.org/browser/browser/base/content/test/trackingUI/cookieServer.sjs", "_blank", "width=100,height=100");
    break;
  case "window-close":
      window.win.close();
      window.win = null;
    break;
  }
};
