/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FormData"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource:///modules/sessionstore/XPathGenerator.jsm");

/**
 * Returns whether the given URL very likely has input
 * fields that contain serialized session store data.
 */
function isRestorationPage(url) {
  return url == "about:sessionrestore" || url == "about:welcomeback";
}

/**
 * Returns whether the given form |data| object contains nested restoration
 * data for a page like about:sessionrestore or about:welcomeback.
 */
function hasRestorationData(data) {
  if (isRestorationPage(data.url) && data.id) {
    return typeof(data.id.sessionData) == "object";
  }

  return false;
}

/**
 * Returns the given document's current URI and strips
 * off the URI's anchor part, if any.
 */
function getDocumentURI(doc) {
  return doc.documentURI.replace(/#.*$/, "");
}

/**
 * The public API exported by this module that allows to collect
 * and restore form data for a document and its subframes.
 */
this.FormData = Object.freeze({
  collect: function (frame) {
    return FormDataInternal.collect(frame);
  },

  restore: function (frame, data) {
    FormDataInternal.restore(frame, data);
  },

  restoreTree: function (root, data) {
    FormDataInternal.restoreTree(root, data);
  }
});

/**
 * This module's internal API.
 */
let FormDataInternal = {
  /**
   * Collect form data for a given |frame| *not* including any subframes.
   *
   * The returned object may have an "id", "xpath", or "innerHTML" key or a
   * combination of those three. Form data stored under "id" is for input
   * fields with id attributes. Data stored under "xpath" is used for input
   * fields that don't have a unique id and need to be queried using XPath.
   * The "innerHTML" key is used for editable documents (designMode=on).
   *
   * Example:
   *   {
   *     id: {input1: "value1", input3: "value3"},
   *     xpath: {
   *       "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value2",
   *       "/xhtml:html/xhtml:body/xhtml:input[@name='input4']" : "value4"
   *     }
   *   }
   *
   * @param  doc
   *         DOMDocument instance to obtain form data for.
   * @return object
   *         Form data encoded in an object.
   */
  collect: function ({document: doc}) {
    let formNodes = doc.evaluate(
      XPathGenerator.restorableFormNodes,
      doc,
      XPathGenerator.resolveNS,
      Ci.nsIDOMXPathResult.UNORDERED_NODE_ITERATOR_TYPE, null
    );

    let node;
    let ret = {};

    // Limit the number of XPath expressions for performance reasons. See
    // bug 477564.
    const MAX_TRAVERSED_XPATHS = 100;
    let generatedCount = 0;

    while (node = formNodes.iterateNext()) {
      let hasDefaultValue = true;
      let value;

      // Only generate a limited number of XPath expressions for perf reasons
      // (cf. bug 477564)
      if (!node.id && generatedCount > MAX_TRAVERSED_XPATHS) {
        continue;
      }

      if (node instanceof Ci.nsIDOMHTMLInputElement ||
          node instanceof Ci.nsIDOMHTMLTextAreaElement ||
          node instanceof Ci.nsIDOMXULTextBoxElement) {
        switch (node.type) {
          case "checkbox":
          case "radio":
            value = node.checked;
            hasDefaultValue = value == node.defaultChecked;
            break;
          case "file":
            value = { type: "file", fileList: node.mozGetFileNameArray() };
            hasDefaultValue = !value.fileList.length;
            break;
          default: // text, textarea
            value = node.value;
            hasDefaultValue = value == node.defaultValue;
            break;
        }
      } else if (!node.multiple) {
        // <select>s without the multiple attribute are hard to determine the
        // default value, so assume we don't have the default.
        hasDefaultValue = false;
        value = { selectedIndex: node.selectedIndex, value: node.value };
      } else {
        // <select>s with the multiple attribute are easier to determine the
        // default value since each <option> has a defaultSelected property
        let options = Array.map(node.options, opt => {
          hasDefaultValue = hasDefaultValue && (opt.selected == opt.defaultSelected);
          return opt.selected ? opt.value : -1;
        });
        value = options.filter(ix => ix > -1);
      }

      // In order to reduce XPath generation (which is slow), we only save data
      // for form fields that have been changed. (cf. bug 537289)
      if (hasDefaultValue) {
        continue;
      }

      if (node.id) {
        ret.id = ret.id || {};
        ret.id[node.id] = value;
      } else {
        generatedCount++;
        ret.xpath = ret.xpath || {};
        ret.xpath[XPathGenerator.generate(node)] = value;
      }
    }

    // designMode is undefined e.g. for XUL documents (as about:config)
    if ((doc.designMode || "") == "on" && doc.body) {
      ret.innerHTML = doc.body.innerHTML;
    }

    // Return |null| if no form data has been found.
    if (Object.keys(ret).length === 0) {
      return null;
    }

    // Store the frame's current URL with its form data so that we can compare
    // it when restoring data to not inject form data into the wrong document.
    ret.url = getDocumentURI(doc);

    // We want to avoid saving data for about:sessionrestore as a string.
    // Since it's stored in the form as stringified JSON, stringifying further
    // causes an explosion of escape characters. cf. bug 467409
    if (isRestorationPage(ret.url)) {
      ret.id.sessionData = JSON.parse(ret.id.sessionData);
    }

    return ret;
  },

  /**
   * Restores form |data| for the given frame. The data is expected to be in
   * the same format that FormData.collect() returns.
   *
   * @param frame (DOMWindow)
   *        The frame to restore form data to.
   * @param data (object)
   *        An object holding form data.
   */
  restore: function ({document: doc}, data) {
    // Don't restore any data for the given frame if the URL
    // stored in the form data doesn't match its current URL.
    if (!data.url || data.url != getDocumentURI(doc)) {
      return;
    }

    // For about:{sessionrestore,welcomeback} we saved the field as JSON to
    // avoid nested instances causing humongous sessionstore.js files.
    // cf. bug 467409
    if (hasRestorationData(data)) {
      data.id.sessionData = JSON.stringify(data.id.sessionData);
    }

    if ("id" in data) {
      let retrieveNode = id => doc.getElementById(id);
      this.restoreManyInputValues(data.id, retrieveNode);
    }

    if ("xpath" in data) {
      let retrieveNode = xpath => XPathGenerator.resolve(doc, xpath);
      this.restoreManyInputValues(data.xpath, retrieveNode);
    }

    if ("innerHTML" in data) {
      // We know that the URL matches data.url right now, but the user
      // may navigate away before the setTimeout handler runs. We do
      // a simple comparison against savedURL to check for that.
      let savedURL = doc.documentURI;

      setTimeout(() => {
        if (doc.body && doc.designMode == "on" && doc.documentURI == savedURL) {
          doc.body.innerHTML = data.innerHTML;
        }
      });
    }
  },

  /**
   * Iterates the given form data, retrieving nodes for all the keys and
   * restores their appropriate values.
   *
   * @param data (object)
   *        A subset of the form data as collected by FormData.collect(). This
   *        is either data stored under "id" or under "xpath".
   * @param retrieve (function)
   *        The function used to retrieve the input field belonging to a key
   *        in the given |data| object.
   */
  restoreManyInputValues: function (data, retrieve) {
    for (let key of Object.keys(data)) {
      let input = retrieve(key);
      if (input) {
        this.restoreSingleInputValue(input, data[key]);
      }
    }
  },

  /**
   * Restores a given form value to a given DOMNode and takes care of firing
   * the appropriate DOM event should the input's value change.
   *
   * @param  aNode
   *         DOMNode to set form value on.
   * @param  aValue
   *         Value to set form element to.
   */
  restoreSingleInputValue: function (aNode, aValue) {
    let eventType;

    if (typeof aValue == "string" && aNode.type != "file") {
      // Don't dispatch an input event if there is no change.
      if (aNode.value == aValue) {
        return;
      }

      aNode.value = aValue;
      eventType = "input";
    } else if (typeof aValue == "boolean") {
      // Don't dispatch a change event for no change.
      if (aNode.checked == aValue) {
        return;
      }

      aNode.checked = aValue;
      eventType = "change";
    } else if (aValue && aValue.selectedIndex >= 0 && aValue.value) {
      // Don't dispatch a change event for no change
      if (aNode.options[aNode.selectedIndex].value == aValue.value) {
        return;
      }

      // find first option with matching aValue if possible
      for (let i = 0; i < aNode.options.length; i++) {
        if (aNode.options[i].value == aValue.value) {
          aNode.selectedIndex = i;
          eventType = "change";
          break;
        }
      }
    } else if (aValue && aValue.fileList && aValue.type == "file" &&
      aNode.type == "file") {
      aNode.mozSetFileNameArray(aValue.fileList, aValue.fileList.length);
      eventType = "input";
    } else if (Array.isArray(aValue) && aNode.options) {
      Array.forEach(aNode.options, function(opt, index) {
        // don't worry about malformed options with same values
        opt.selected = aValue.indexOf(opt.value) > -1;

        // Only fire the event here if this wasn't selected by default
        if (!opt.defaultSelected) {
          eventType = "change";
        }
      });
    }

    // Fire events for this node if applicable
    if (eventType) {
      let doc = aNode.ownerDocument;
      let event = doc.createEvent("UIEvents");
      event.initUIEvent(eventType, true, true, doc.defaultView, 0);
      aNode.dispatchEvent(event);
    }
  },

  /**
   * Restores form data for the current frame hierarchy starting at |root|
   * using the given form |data|.
   *
   * If the given |root| frame's hierarchy doesn't match that of the given
   * |data| object we will silently discard data for unreachable frames. For
   * security reasons we will never restore form data to the wrong frames as
   * we bail out silently if the stored URL doesn't match the frame's current
   * URL.
   *
   * @param root (DOMWindow)
   * @param data (object)
   *        {
   *          formdata: {id: {input1: "value1"}},
   *          children: [
   *            {formdata: {id: {input2: "value2"}}},
   *            null,
   *            {formdata: {xpath: { ... }}, children: [ ... ]}
   *          ]
   *        }
   */
  restoreTree: function (root, data) {
    // Don't restore any data for the root frame and its subframes if there
    // is a URL stored in the form data and it doesn't match its current URL.
    if (data.url && data.url != getDocumentURI(root.document)) {
      return;
    }

    if (data.url) {
      this.restore(root, data);
    }

    if (!data.hasOwnProperty("children")) {
      return;
    }

    let frames = root.frames;
    for (let index of Object.keys(data.children)) {
      if (index < frames.length) {
        this.restoreTree(frames[index], data.children[index]);
      }
    }
  }
};
