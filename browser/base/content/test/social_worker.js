/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let testPort, sidebarPort;

onconnect = function(e) {
  let port = e.ports[0];
  port.onmessage = function onMessage(event) {
    let topic = event.data.topic;
    switch (topic) {
      case "test-init":
        testPort = port;
        break;
      case "sidebar-message":
        sidebarPort = port;
        if (testPort && event.data.result == "ok")
          testPort.postMessage({topic:"got-sidebar-message"});
        break;
      case "service-window-message":
        testPort.postMessage({topic:"got-service-window-message"});
        break;
      case "service-window-closed-message":
        testPort.postMessage({topic:"got-service-window-closed-message"});
        break;
      case "test-service-window":
        sidebarPort.postMessage({topic:"test-service-window"});
        break;
      case "test-service-window-twice":
        sidebarPort.postMessage({topic:"test-service-window-twice"});
        break;
      case "test-service-window-twice-result":
        testPort.postMessage({topic: "test-service-window-twice-result", result: event.data.result })
        break;
      case "test-close-service-window":
        sidebarPort.postMessage({topic:"test-close-service-window"});
        break;
      case "panel-message":
        if (testPort && event.data.result == "ok")
          testPort.postMessage({topic:"got-panel-message"});
        break;
      case "social.initialize":
        // This is the workerAPI port, respond and set up a notification icon.
        port.postMessage({topic: "social.initialize-response"});
        let profile = {
          userName: "foo"
        };
        port.postMessage({topic: "social.user-profile", data: profile});
        let icon = {
          name: "testIcon",
          iconURL: "chrome://branding/content/icon48.png",
          contentPanel: "http://example.com/browser/browser/base/content/test/social_panel.html",
          counter: 1
        };
        port.postMessage({topic: "social.ambient-notification", data: icon});
        break;
    }
  }
}
