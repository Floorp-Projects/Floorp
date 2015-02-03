/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { query, constant, cache } = require("sdk/lang/functional");
const { pairs, each, map, object } = require("sdk/util/sequence");
const { nodeToMessageManager } = require("./util");

// Decorator function that takes `f` function and returns one that attempts
// to run `f` with given arguments. In case of exception error is logged
// and `fallback` is returned instead.
const Try = (fn, fallback=null) => (...args) => {
  try {
    return fn(...args);
  } catch(error) {
    console.error(error);
    return fallback;
  }
};

// Decorator funciton that takes `f` function and returns one that returns
// JSON cloned result of whatever `f` returns for given arguments.
const JSONReturn = f => (...args) => JSON.parse(JSON.stringify(f(...args)));

const Null = constant(null);

// Table of readers mapped to field names they're going to be reading.
const readers = Object.create(null);
// Read function takes "contextmenu" event target `node` and returns table of
// read field names mapped to appropriate values. Read uses above defined read
// table to read data for all registered readers.
const read = node =>
  object(...map(([id, read]) => [id, read(node, id)], pairs(readers)));

// Table of built-in readers, each takes a descriptor and returns a reader:
// descriptor -> node -> JSON
const parsers = Object.create(null)
// Function takes a descriptor of the remotely defined reader and parsese it
// to construct a local reader that's going to read out data from context menu
// target.
const parse = descriptor => {
  const parser = parsers[descriptor.category];
  if (!parser) {
    console.error("Unknown reader descriptor was received", descriptor, `"${descriptor.category}"`);
    return Null
  }
  return Try(parser(descriptor));
}

// TODO: Test how chrome's mediaType behaves to try and match it's behavior.
const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const SVG_NS = "http://www.w3.org/2000/svg";

// Firefox always creates a HTMLVideoElement when loading an ogg file
// directly. If the media is actually audio, be smarter and provide a
// context menu with audio operations.
// Source: https://github.com/mozilla/gecko-dev/blob/28c2fca3753c5371643843fc2f2f205146b083b7/browser/base/content/nsContextMenu.js#L632-L637
const isVideoLoadingAudio = node =>
  node.readyState >= node.HAVE_METADATA &&
    (node.videoWidth == 0 || node.videoHeight == 0)

const isVideo = node =>
  node instanceof node.ownerDocument.defaultView.HTMLVideoElement &&
  !isVideoLoadingAudio(node);

const isAudio = node => {
  const {HTMLVideoElement, HTMLAudioElement} = node.ownerDocument.defaultView;
  return node instanceof HTMLAudioElement ? true :
         node instanceof HTMLVideoElement ? isVideoLoadingAudio(node) :
         false;
};

const isImage = ({namespaceURI, localName}) =>
  namespaceURI === HTML_NS && localName === "img" ? true :
  namespaceURI === XUL_NS && localName === "image" ? true :
  namespaceURI === SVG_NS && localName === "image" ? true :
  false;

parsers["reader/MediaType()"] = constant(node =>
  isImage(node) ? "image" :
  isAudio(node) ? "audio" :
  isVideo(node) ? "video" :
  null);


const readLink = node =>
  node.namespaceURI === HTML_NS && node.localName === "a" ? node.href :
  readLink(node.parentNode);

parsers["reader/LinkURL()"] = constant(node =>
  node.matches("a, a *") ? readLink(node) : null);

// Reader that reads out `true` if "contextmenu" `event.target` matches
// `descriptor.selector` and `false` if it does not.
parsers["reader/SelectorMatch()"] = ({selector}) =>
  node => node.matches(selector);

// Accessing `selectionStart` and `selectionEnd` properties on non
// editable input nodes throw exceptions, there for we need this util
// function to guard us against them.
const getInputSelection = node => {
  try {
    if ("selectionStart" in node && "selectionEnd" in node) {
      const {selectionStart, selectionEnd} = node;
      return {selectionStart, selectionEnd}
    }
  }
  catch(_) {}

  return null;
}

// Selection reader does not really cares about descriptor so it is
// a constant function returning selection reader. Selection reader
// returns string of the selected text or `null` if there is no selection.
parsers["reader/Selection()"] = constant(node => {
  const selection = node.ownerDocument.getSelection();
  if (!selection.isCollapsed) {
    return selection.toString();
  }
  // If target node is editable (text, input, textarea, etc..) document does
  // not really handles selections there. There for we fallback to checking
  // `selectionStart` `selectionEnd` properties and if they are present we
  // extract selections manually from the `node.value`.
  else {
    const selection = getInputSelection(node);
    const isSelected = selection &&
                       Number.isInteger(selection.selectionStart) &&
                       Number.isInteger(selection.selectionEnd) &&
                       selection.selectionStart !== selection.selectionEnd;
    return  isSelected ? node.value.substring(selection.selectionStart,
                                              selection.selectionEnd) :
            null;
  }
});

// Query reader just reads out properties from the node, so we just use `query`
// utility function.
parsers["reader/Query()"] = ({path}) => JSONReturn(query(path));
// Attribute reader just reads attribute of the event target node.
parsers["reader/Attribute()"] = ({name}) => node => node.getAttribute(name);

// Extractor reader defines generates a reader out of serialized function, who's
// return value is JSON cloned. Note: We do know source will evaluate to function
// as that's what we serialized on the other end, it's also ok if generated function
// is going to throw as registered readers are wrapped in try catch to avoid breakting
// unrelated readers.
parsers["reader/Extractor()"] = ({source}) =>
  JSONReturn(new Function("return (" + source + ")")());

// If the context-menu target node or any of its ancestors is one of these,
// Firefox uses a tailored context menu, and so the page context doesn't apply.
// There for `reader/isPage()` will read `false` in that case otherwise it's going
// to read `true`.
const nonPageElements = ["a", "applet", "area", "button", "canvas", "object",
                         "embed", "img", "input", "map", "video", "audio", "menu",
                         "option", "select", "textarea", "[contenteditable=true]"];
const nonPageSelector = nonPageElements.
                          concat(nonPageElements.map(tag => `${tag} *`)).
                          join(", ");

// Note: isPageContext implementation could have actually used SelectorMatch reader,
// but old implementation was also checked for collapsed selection there for to keep
// the behavior same we end up implementing a new reader.
parsers["reader/isPage()"] = constant(node =>
  node.ownerDocument.defaultView.getSelection().isCollapsed &&
  !node.matches(nonPageSelector));

// Reads `true` if node is in an iframe otherwise returns true.
parsers["reader/isFrame()"] = constant(node =>
  !!node.ownerDocument.defaultView.frameElement);

parsers["reader/isEditable()"] = constant(node => {
  const selection = getInputSelection(node);
  return selection ? !node.readOnly && !node.disabled : node.isContentEditable;
});


// TODO: Add some reader to read out tab id.

const onReadersUpdate = message => {
  each(([id, descriptor]) => {
    if (descriptor) {
      readers[id] = parse(descriptor);
    }
    else {
      delete readers[id];
    }
  }, pairs(message.data));
};
exports.onReadersUpdate = onReadersUpdate;


const onContextMenu = event => {
  if (!event.defaultPrevented) {
    const manager = nodeToMessageManager(event.target);
    manager.sendSyncMessage("sdk/context-menu/read", read(event.target), readers);
  }
};
exports.onContextMenu = onContextMenu;


const onContentFrame = (frame) => {
  // Listen for contextmenu events in on this frame.
  frame.addEventListener("contextmenu", onContextMenu);
  // Listen to registered reader changes and update registry.
  frame.addMessageListener("sdk/context-menu/readers", onReadersUpdate);

  // Request table of readers (if this is loaded in a new process some table
  // changes may be missed, this is way to sync up).
  frame.sendAsyncMessage("sdk/context-menu/readers?");
};
exports.onContentFrame = onContentFrame;
