/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { render } = require("devtools/client/shared/vendor/react-dom");
  const { createFactories } = require("devtools/client/shared/react-utils");
  const { MainTabbedArea } = createFactories(
    require("devtools/client/jsonview/components/MainTabbedArea")
  );
  const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");

  const AUTO_EXPAND_MAX_SIZE = 100 * 1024;
  const AUTO_EXPAND_MAX_LEVEL = 7;
  const TABS = {
    JSON: 0,
    RAW_DATA: 1,
    HEADERS: 2,
  };

  let prettyURL;
  let theApp;

  // Application state object.
  const input = {
    jsonText: JSONView.json,
    jsonPretty: null,
    headers: JSONView.headers,
    activeTab: 0,
    prettified: false,
    expandedNodes: new Set(),
  };

  /**
   * Application actions/commands. This list implements all commands
   * available for the JSON viewer.
   */
  input.actions = {
    onCopyJson() {
      const text = input.prettified ? input.jsonPretty : input.jsonText;
      copyString(text.textContent);
    },

    onSaveJson() {
      if (input.prettified && !prettyURL) {
        prettyURL = URL.createObjectURL(
          new window.Blob([input.jsonPretty.textContent])
        );
      }
      dispatchEvent("save", input.prettified ? prettyURL : null);
    },

    onCopyHeaders() {
      let value = "";
      const isWinNT =
        document.documentElement.getAttribute("platform") === "win";
      const eol = isWinNT ? "\r\n" : "\n";

      const responseHeaders = input.headers.response;
      for (let i = 0; i < responseHeaders.length; i++) {
        const header = responseHeaders[i];
        value += header.name + ": " + header.value + eol;
      }

      value += eol;

      const requestHeaders = input.headers.request;
      for (let i = 0; i < requestHeaders.length; i++) {
        const header = requestHeaders[i];
        value += header.name + ": " + header.value + eol;
      }

      copyString(value);
    },

    onSearch(value) {
      theApp.setState({ searchFilter: value });
    },

    onPrettify(data) {
      if (input.json instanceof Error) {
        // Cannot prettify invalid JSON
        return;
      }
      if (input.prettified) {
        theApp.setState({ jsonText: input.jsonText });
      } else {
        if (!input.jsonPretty) {
          input.jsonPretty = new Text(JSON.stringify(input.json, null, "  "));
        }
        theApp.setState({ jsonText: input.jsonPretty });
      }

      input.prettified = !input.prettified;
    },

    onCollapse(data) {
      input.expandedNodes.clear();
      theApp.forceUpdate();
    },

    onExpand(data) {
      input.expandedNodes = TreeViewClass.getExpandedNodes(input.json);
      theApp.setState({ expandedNodes: input.expandedNodes });
    },
  };

  /**
   * Helper for copying a string to the clipboard.
   *
   * @param {String} string The text to be copied.
   */
  function copyString(string) {
    document.addEventListener(
      "copy",
      event => {
        event.clipboardData.setData("text/plain", string);
        event.preventDefault();
      },
      { once: true }
    );

    document.execCommand("copy", false, null);
  }

  /**
   * Helper for dispatching an event. It's handled in chrome scope.
   *
   * @param {String} type Event detail type
   * @param {Object} value Event detail value
   */
  function dispatchEvent(type, value) {
    const data = {
      detail: {
        type,
        value,
      },
    };

    const contentMessageEvent = new CustomEvent("contentMessage", data);
    window.dispatchEvent(contentMessageEvent);
  }

  /**
   * Render the main application component. It's the main tab bar displayed
   * at the top of the window. This component also represents ReacJS root.
   */
  const content = document.getElementById("content");
  const promise = (async function parseJSON() {
    if (document.readyState == "loading") {
      // If the JSON has not been loaded yet, render the Raw Data tab first.
      input.json = {};
      input.activeTab = TABS.RAW_DATA;
      return new Promise(resolve => {
        document.addEventListener("DOMContentLoaded", resolve, { once: true });
      })
        .then(parseJSON)
        .then(async () => {
          // Now update the state and switch to the JSON tab.
          await appIsReady;
          theApp.setState({
            activeTab: TABS.JSON,
            json: input.json,
            expandedNodes: input.expandedNodes,
          });
        });
    }

    // If the JSON has been loaded, parse it immediately before loading the app.
    const jsonString = input.jsonText.textContent;
    try {
      input.json = JSON.parse(jsonString);
    } catch (err) {
      input.json = err;
      // Display the raw data tab for invalid json
      input.activeTab = TABS.RAW_DATA;
    }

    // Expand the document by default if its size isn't bigger than 100KB.
    if (
      !(input.json instanceof Error) &&
      jsonString.length <= AUTO_EXPAND_MAX_SIZE
    ) {
      input.expandedNodes = TreeViewClass.getExpandedNodes(input.json, {
        maxLevel: AUTO_EXPAND_MAX_LEVEL,
      });
    }
    return undefined;
  })();

  const appIsReady = new Promise(resolve => {
    render(MainTabbedArea(input), content, function () {
      theApp = this;
      resolve();

      // Send readyState change notification event to the window. Can be useful for
      // tests as well as extensions.
      JSONView.readyState = "interactive";
      window.dispatchEvent(new CustomEvent("AppReadyStateChange"));

      promise.then(() => {
        // Another readyState change notification event.
        JSONView.readyState = "complete";
        window.dispatchEvent(new CustomEvent("AppReadyStateChange"));
      });
    });
  });
});
