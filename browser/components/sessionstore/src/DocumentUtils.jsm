/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "DocumentUtils" ];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/sessionstore/XPathGenerator.jsm");

this.DocumentUtils = {
  /**
   * Obtain form data for a DOMDocument instance.
   *
   * The returned object has 2 keys, "id" and "xpath". Each key holds an object
   * which further defines form data.
   *
   * The "id" object maps element IDs to values. The "xpath" object maps the
   * XPath of an element to its value.
   *
   * @param  aDocument
   *         DOMDocument instance to obtain form data for.
   * @return object
   *         Form data encoded in an object.
   */
  getFormData: function DocumentUtils_getFormData(aDocument) {
    let formNodes = aDocument.evaluate(
      XPathGenerator.restorableFormNodes,
      aDocument,
      XPathGenerator.resolveNS,
      Ci.nsIDOMXPathResult.UNORDERED_NODE_ITERATOR_TYPE, null
    );

    let node;
    let ret = {id: {}, xpath: {}};

    // Limit the number of XPath expressions for performance reasons. See
    // bug 477564.
    const MAX_TRAVERSED_XPATHS = 100;
    let generatedCount = 0;

    while (node = formNodes.iterateNext()) {
      let nId = node.id;
      let hasDefaultValue = true;
      let value;

      // Only generate a limited number of XPath expressions for perf reasons
      // (cf. bug 477564)
      if (!nId && generatedCount > MAX_TRAVERSED_XPATHS) {
        continue;
      }

      if (node instanceof Ci.nsIDOMHTMLInputElement ||
          node instanceof Ci.nsIDOMHTMLTextAreaElement) {
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
        // default value since each <option> has a defaultSelected
        let options = Array.map(node.options, function(aOpt, aIx) {
          let oSelected = aOpt.selected;
          hasDefaultValue = hasDefaultValue && (oSelected == aOpt.defaultSelected);
          return oSelected ? aOpt.value : -1;
        });
        value = options.filter(function(aIx) aIx !== -1);
      }

      // In order to reduce XPath generation (which is slow), we only save data
      // for form fields that have been changed. (cf. bug 537289)
      if (!hasDefaultValue) {
        if (nId) {
          ret.id[nId] = value;
        } else {
          generatedCount++;
          ret.xpath[XPathGenerator.generate(node)] = value;
        }
      }
    }

    return ret;
  },

  /**
   * Merges form data on a document from previously obtained data.
   *
   * This is the inverse of getFormData(). The data argument is the same object
   * type which is returned by getFormData(): an object containing the keys
   * "id" and "xpath" which are each objects mapping element identifiers to
   * form values.
   *
   * Where the document has existing form data for an element, the value
   * will be replaced. Where the document has a form element but no matching
   * data in the passed object, the element is untouched.
   *
   * @param  aDocument
   *         DOMDocument instance to which to restore form data.
   * @param  aData
   *         Object defining form data.
   */
  mergeFormData: function DocumentUtils_mergeFormData(aDocument, aData) {
    if ("xpath" in aData) {
      for each (let [xpath, value] in Iterator(aData.xpath)) {
        let node = XPathGenerator.resolve(aDocument, xpath);

        if (node) {
          this.restoreFormValue(node, value, aDocument);
        }
      }
    }

    if ("id" in aData) {
      for each (let [id, value] in Iterator(aData.id)) {
        let node = aDocument.getElementById(id);

        if (node) {
          this.restoreFormValue(node, value, aDocument);
        }
      }
    }
  },

  /**
   * Low-level function to restore a form value to a DOMNode.
   *
   * If you want a higher-level interface, see mergeFormData().
   *
   * When the value is changed, the function will fire the appropriate DOM
   * events.
   *
   * @param  aNode
   *         DOMNode to set form value on.
   * @param  aValue
   *         Value to set form element to.
   * @param  aDocument [optional]
   *         DOMDocument node belongs to. If not defined, node.ownerDocument
   *         is used.
   */
  restoreFormValue: function DocumentUtils_restoreFormValue(aNode, aValue, aDocument) {
    aDocument = aDocument || aNode.ownerDocument;

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
    } else if (typeof aValue == "number") {
      // handle select backwards compatibility, example { "#id" : index }
      // We saved the value blindly since selects take more work to determine
      // default values. So now we should check to avoid unnecessary events.
      if (aNode.selectedIndex == aValue) {
        return;
      }

      if (aValue < aNode.options.length) {
        aNode.selectedIndex = aValue;
        eventType = "change";
      }
    } else if (aValue && aValue.selectedIndex >= 0 && aValue.value) {
      // handle select new format

      // Don't dispatch a change event for no change
      if (aNode.options[aNode.selectedIndex].value == aValue.value) {
        return;
      }

      // find first option with matching aValue if possible
      for (let i = 0; i < aNode.options.length; i++) {
        if (aNode.options[i].value == aValue.value) {
          aNode.selectedIndex = i;
          break;
        }
      }
      eventType = "change";
    } else if (aValue && aValue.fileList && aValue.type == "file" &&
      aNode.type == "file") {
      aNode.mozSetFileNameArray(aValue.fileList, aValue.fileList.length);
      eventType = "input";
    } else if (aValue && typeof aValue.indexOf == "function" && aNode.options) {
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
      let event = aDocument.createEvent("UIEvents");
      event.initUIEvent(eventType, true, true, aDocument.defaultView, 0);
      aNode.dispatchEvent(event);
    }
  }
};
