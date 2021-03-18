/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals XPCNativeWrapper */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

// URL constructor doesn't support about: scheme
const href = window.location.href.replace("about:", "http://");
const url = new window.URL(href);

// `host` is the frame element loading the toolbox.
let host = window.browsingContext.embedderElement;

// If there's no containerElement (which happens when loading about:devtools-toolbox as
// a top level document), use the current window.
if (!host) {
  host = {
    contentWindow: window,
    contentDocument: document,
    // toolbox-host-manager.js wants to set attributes on the frame that contains it,
    // but that is fine to skip and doesn't make sense when using the current window.
    setAttribute() {},
    ownerDocument: document,
    // toolbox-host-manager.js wants to listen for unload events from outside the frame,
    // but this is fine to skip since the toolbox code listens inside the frame as well,
    // and there is no outer document in this case.
    addEventListener() {},
  };
}

const onLoad = new Promise(r => {
  host.contentWindow.addEventListener("DOMContentLoaded", r, { once: true });
});

async function showErrorPage(doc, errorMessage) {
  const win = doc.defaultView;
  const { BrowserLoader } = ChromeUtils.import(
    "resource://devtools/client/shared/browser-loader.js"
  );
  const browserRequire = BrowserLoader({
    window: win,
    useOnlyShared: true,
  }).require;

  const React = browserRequire("devtools/client/shared/vendor/react");
  const ReactDOM = browserRequire("devtools/client/shared/vendor/react-dom");
  const DebugTargetErrorPage = React.createFactory(
    require("devtools/client/framework/components/DebugTargetErrorPage")
  );
  const { LocalizationHelper } = browserRequire("devtools/shared/l10n");
  const L10N = new LocalizationHelper(
    "devtools/client/locales/toolbox.properties"
  );

  // mount the React component into our XUL container once the DOM is ready
  await onLoad;

  // Update the tab title.
  document.title = L10N.getStr("toolbox.debugTargetInfo.tabTitleError");

  const mountEl = doc.querySelector("#toolbox-error-mount");
  const element = DebugTargetErrorPage({
    errorMessage,
    L10N,
  });
  ReactDOM.render(element, mountEl);

  // make sure we unmount the component when the page is destroyed
  win.addEventListener(
    "unload",
    () => {
      ReactDOM.unmountComponentAtNode(mountEl);
    },
    { once: true }
  );
}

async function initToolbox(url, host) {
  const { gDevTools } = require("devtools/client/framework/devtools");

  const {
    descriptorFromURL,
  } = require("devtools/client/framework/descriptor-from-url");
  const { Toolbox } = require("devtools/client/framework/toolbox");

  // Specify the default tool to open
  const tool = url.searchParams.get("tool");

  try {
    let descriptor;
    if (url.searchParams.has("target")) {
      descriptor = await _createTestOnlyDescriptor(host);
    } else {
      descriptor = await descriptorFromURL(url);
      const toolbox = gDevTools.getToolboxForDescriptor(descriptor);
      if (toolbox && toolbox.isDestroying()) {
        // If a toolbox already exists for the descriptor, wait for current
        // toolbox destroy to be finished.
        await toolbox.destroy();
      }
    }

    const options = { customIframe: host };
    const newToolbox = await gDevTools.showToolbox(descriptor, {
      toolId: tool,
      hostType: Toolbox.HostType.PAGE,
      hostOptions: options,
    });

    // TODO: We should use an event from the descriptor instead, in order to
    // attach to it before the toolbox was opened. Otherwise if a target
    // disconnects before the toolbox was fully displayed, we will not navigate
    // to the error page. See https://bugzilla.mozilla.org/show_bug.cgi?id=1695929
    const target = newToolbox.target;
    // Display an error page if we are connected to a remote target and we lose it
    const { descriptorFront } = target;
    descriptorFront.once("descriptor-destroyed", function() {
      // Prevent trying to display the error page if the toolbox tab is being destroyed
      if (host.contentDocument) {
        const error = new Error("Debug target was disconnected");
        showErrorPage(host.contentDocument, `${error}`);
      }
    });

    // If this isn't a cached client, it means that we just created a new client
    // in `clientFromURL` and we have to destroy it at some point.
    // In such case, force the Target to destroy the client as soon as it gets
    // destroyed. This typically happens only for about:debugging toolboxes
    // opened for local Firefox's targets.
    const isCachedClient = url.searchParams.get("remoteId");
    target.shouldCloseClient = !isCachedClient;
  } catch (error) {
    // When an error occurs, show error page with message.
    console.error("Exception while loading the toolbox", error);
    showErrorPage(host.contentDocument, `${error}`);
  }
}

/**
 * Attach toolbox to a given browser iframe (<xul:browser> or <html:iframe
 * mozbrowser>) whose reference is set on the host iframe.
 *
 * Note that there is no real usage of it. It is only used by the test found at
 * devtools/client/framework/test/browser_toolbox_target.js.
 */
async function _createTestOnlyDescriptor(host) {
  const { DevToolsServer } = require("devtools/server/devtools-server");
  const { DevToolsClient } = require("devtools/client/devtools-client");

  // `iframe` is the targeted document to debug
  let iframe = host.wrappedJSObject ? host.wrappedJSObject.target : host.target;
  if (!iframe) {
    throw new Error("Unable to find the targeted iframe to debug");
  }

  // Need to use a xray to have attributes and behavior expected by
  // devtools codebase
  iframe = XPCNativeWrapper(iframe);

  // Fake a xul:tab object as we don't have one.
  // linkedBrowser is the only one attribute being queried by client.getTab
  const tab = { linkedBrowser: iframe };

  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  const client = new DevToolsClient(DevToolsServer.connectPipe());

  await client.connect();
  // Creates a target for a given browser iframe.
  const descriptor = await client.mainRoot.getTab({ tab });

  // XXX: Normally we don't need to fetch the target anymore, but the test
  // listens to an early event `toolbox-ready` which will kick in before
  // the rest of `initToolbox` can be done.
  const target = await descriptor.getTarget();
  // Instruct the Target to automatically close the client on destruction.
  target.shouldCloseClient = true;

  return descriptor;
}

// Only use this method to attach the toolbox if some query parameters are given
if (url.search.length > 1) {
  initToolbox(url, host);
}
// TODO: handle no params in about:devtool-toolbox
// https://bugzilla.mozilla.org/show_bug.cgi?id=1526996
