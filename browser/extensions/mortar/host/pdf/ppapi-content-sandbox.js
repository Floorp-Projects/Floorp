/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This code runs in the sandbox in the content process where the page that
 * loaded the plugin lives. It communicates with the process where the PPAPI
 * implementation lives.
 */
const { utils: Cu } = Components;

let mm = pluginElement.frameLoader.messageManager;
let containerWindow = pluginElement.ownerDocument.defaultView;
// Prevent the drag event's default action on the element, to avoid dragging of
// that element while the user selects text.
pluginElement.addEventListener("dragstart",
                               function(event) { event.preventDefault(); });
// For synthetic documents only, prevent the select event's default action on
// the element, to avoid selecting the element and the copying of an empty
// string into the clipboard. Don't do this for non-synthetic documents, as
// users still need to be able to select text outside the plugin.
let isSynthetic = pluginElement.ownerDocument.mozSyntheticDocument;
if (isSynthetic) {
  pluginElement.ownerDocument.body.addEventListener(
    "selectstart", function(event) { event.preventDefault(); });
}

function mapValue(v, instance) {
  return instance.rt.toPP_Var(v, instance);
}

dump("<>>>>>>>>>>>>>>>>>>>> AHA <<<<<<<<<<<<<<<<<<<<<>\n");
dump(`pluginElement: ${pluginElement.toSource()}\n`);
dump(`pluginElement.frameLoader: ${pluginElement.frameLoader.toSource()}\n`);
dump(`pluginElement.frameLoader.messageManager: ${pluginElement.frameLoader.messageManager.toSource()}\n`);
dump("<>>>>>>>>>>>>>>>>>>>> AHA2 <<<<<<<<<<<<<<<<<<<<<>\n");

mm.addMessageListener("ppapi.js:frameLoaded", ({ target }) => {
  let tagName = pluginElement.nodeName;

  // Getting absolute URL from the EMBED tag
  let url = pluginElement.srcURI.spec;
  let objectParams = new Map();
  for (let i = 0; i < pluginElement.attributes.length; ++i) {
    let paramName = pluginElement.attributes[i].localName;
    objectParams.set(paramName, pluginElement.attributes[i].value);
  }
  if (tagName == "OBJECT") {
    let params = pluginElement.getElementsByTagName("param");
    Array.prototype.forEach.call(params, (p) => {
      var paramName = p.getAttribute("name").toLowerCase();
      objectParams.set(paramName, p.getAttribute("value"));
    });
  }

  let documentURL = pluginElement.ownerDocument.location.href;
  let baseUrl = documentURL;
  if (objectParams.base) {
    try {
      let parsedDocumentUrl = Services.io.newURI(documentURL);
      baseUrl = Services.io.newURI(objectParams.base, null, parsedDocumentUrl).spec;
    } catch (e) { /* ignore */ }
  }

  let info = {
    documentURL: "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai",
    url,
    setupJSInstanceObject: false,
    isFullFrame: false, //pluginElement.ownerDocument.mozSyntheticDocument,
    arguments: {
      keys: ["src", "full-frame", "top-level-url"],
      values: [url, "", documentURL],
    },
  };

  mm.sendAsyncMessage("ppapi.js:createInstance",  { type: "pdf", info },
                      { pluginWindow: containerWindow });

  containerWindow.document.addEventListener("fullscreenchange", () => {
    let fullscreen = (containerWindow.document.fullscreenElement == pluginElement);
    mm.sendAsyncMessage("ppapi.js:fullscreenchange", { fullscreen });
  });
});

mm.addMessageListener("ppapi.js:setFullscreen", ({ data }) => {
  if (data) {
    pluginElement.requestFullscreen();
  } else {
    containerWindow.document.exitFullscreen();
  }
});

mm.loadFrameScript("resource://ppapi.js/ppapi-instance.js", true);
