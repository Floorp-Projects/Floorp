/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });

  let browser = tab.linkedBrowser;
  browser.stop(); // stop the about:blank load

  let writeDomainURL = encodeURI("data:text/html,<script>document.write(document.domain);</script>");
  let tests = [
    {
      name: "view background image",
      url: "http://mochi.test:8888/",
      go: function (cb) {
        let contentBody = browser.contentDocument.body;
        contentBody.style.backgroundImage = "url('" + writeDomainURL + "')";
        doOnLoad(function () {
          let domain = browser.contentDocument.body.textContent;
          is(domain, "", "no domain was inherited for view background image");
          cb();
        });

        let contextMenu = initContextMenu(contentBody);
        contextMenu.viewBGImage();
      }
    },
    {
      name: "view image",
      url: "http://mochi.test:8888/",
      go: function (cb) {
        doOnLoad(function () {
          let domain = browser.contentDocument.body.textContent;
          is(domain, "", "no domain was inherited for view image");
          cb();
        });

        let doc = browser.contentDocument;
        let img = doc.createElement("img");
        img.setAttribute("src", writeDomainURL);
        doc.body.appendChild(img);

        let contextMenu = initContextMenu(img);
        contextMenu.viewMedia();
      }
    },
    {
      name: "show only this frame",
      url: "http://mochi.test:8888/",
      go: function (cb) {
        doOnLoad(function () {
          let domain = browser.contentDocument.body.textContent;
          is(domain, "", "no domain was inherited for 'show only this frame'");
          cb();
        });

        let doc = browser.contentDocument;
        let iframe = doc.createElement("iframe");
        iframe.setAttribute("src", writeDomainURL);
        doc.body.appendChild(iframe);

        iframe.addEventListener("load", function onload() {
          let contextMenu = initContextMenu(iframe.contentDocument.body);
          contextMenu.showOnlyThisFrame();
        }, false);
      }
    }
  ];

  function doOnLoad(cb) {
    browser.addEventListener("load", function onLoad(e) {
      if (e.target != browser.contentDocument)
        return;
      browser.removeEventListener("load", onLoad, true);
      cb();
    }, true);
  }

  function doNext() {
    let test = tests.shift();
    if (test) {
      info("Running test: " + test.name);
      doOnLoad(function () {
        test.go(function () {
          executeSoon(doNext);
        });
      });
      browser.contentDocument.location = test.url;
    } else {
      executeSoon(finish);
    }
  }

  doNext();
}

function initContextMenu(aNode) {
  document.popupNode = aNode;
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let contextMenu = new nsContextMenu(contentAreaContextMenu);
  return contextMenu;
}
