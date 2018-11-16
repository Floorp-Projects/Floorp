onmessage = event => {
  switch (event.data) {
  case "tracking": {
      let ifr = document.createElement("iframe");
      ifr.src = "https://trackertest.org/";
      document.body.appendChild(ifr);
    }
    break;
  case "cookie": {
      let ifr = document.createElement("iframe");
      ifr.src = "https://trackertest.org/browser/browser/base/content/test/trackingUI/cookieServer.sjs";
      document.body.appendChild(ifr);
    }
    break;
  }
};
