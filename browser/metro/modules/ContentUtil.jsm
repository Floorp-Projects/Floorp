/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["ContentUtil"];

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const nsIDOMKeyEvent = Components.interfaces.nsIDOMKeyEvent;

this.ContentUtil = {
  populateFragmentFromString: function populateFragmentFromString(fragment, str) {
    let re = /^([^#]*)#(\d+)\b([^#]*)/,
        document = fragment.ownerDocument,
        // the remaining arguments are our {text, className} values
        replacements = Array.slice(arguments, 2),
        match;

    // walk over the string, building textNode/spans as nec. with replacement content
    // note that #1,#2 etc. may not appear in numerical order in the string
    while ((match = re.exec(str))) {
      let [mstring,pre,num,post] = match,
          replaceText = "",
          replaceClass,
          idx = num-1; // markers are 1-based, replacement indices 0 based

      str = str.substring(re.lastIndex+mstring.length);

      if (pre)
          fragment.appendChild(document.createTextNode(pre));

      if (replacements[idx]) {
        replaceText = replacements[idx].text;
        let spanNode = document.createElementNS(XHTML_NS, "span");
        spanNode.appendChild(document.createTextNode(replaceText));
        // add class to the span when provided
        if(replacements[idx].className)
          spanNode.classList.add(replacements[idx].className);

        fragment.appendChild(spanNode);
      } else {
        // put it back if no replacement was provided
        fragment.appendChild(document.createTextNode("#"+num));
      }

      if(post)
        fragment.appendChild(document.createTextNode(post));
    }
    if(str)
      fragment.appendChild(document.createTextNode(str));

    return fragment;
  },

  // Pass several objects in and it will combine them all into the first object and return it.
  // NOTE: Deep copy is not supported
  extend: function extend() {
    // copy reference to target object
    let target = arguments[0] || {};
    let length = arguments.length;

    if (length === 1) {
      return target;
    }

    // Handle case when target is a string or something
    if (typeof target != "object" && typeof target != "function") {
      target = {};
    }

    for (let i = 1; i < length; i++) {
      // Only deal with non-null/undefined values
      let options = arguments[i];
      if (options != null) {
        // Extend the base object
        for (let name in options) {
          let copy = options[name];

          // Prevent never-ending loop
          if (target === copy)
            continue;

          if (copy !== undefined)
            target[name] = copy;
        }
      }
    }

    // Return the modified object
    return target;
  },

  // Checks if a keycode is used for list navigation.
  isNavigationKey: function (keyCode) {
    let navigationKeys = [
      nsIDOMKeyEvent.DOM_VK_DOWN,
      nsIDOMKeyEvent.DOM_VK_UP,
      nsIDOMKeyEvent.DOM_VK_LEFT,
      nsIDOMKeyEvent.DOM_VK_RIGHT,
      nsIDOMKeyEvent.DOM_VK_PAGE_UP,
      nsIDOMKeyEvent.DOM_VK_PAGE_DOWN,
      nsIDOMKeyEvent.DOM_VK_ESCAPE];

    return navigationKeys.indexOf(keyCode) != -1;
  }
};
