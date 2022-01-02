/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["CommonUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Assert } = ChromeUtils.import("resource://testing-common/Assert.jsm");

const MAX_TRIM_LENGTH = 100;

const CommonUtils = {
  /**
   * Constant passed to getAccessible to indicate that it shouldn't fail if
   * there is no accessible.
   */
  DONOTFAIL_IF_NO_ACC: 1,

  /**
   * Constant passed to getAccessible to indicate that it shouldn't fail if it
   * does not support an interface.
   */
  DONOTFAIL_IF_NO_INTERFACE: 2,

  /**
   * nsIAccessibilityService service.
   */
  get accService() {
    if (!this._accService) {
      this._accService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );
    }

    return this._accService;
  },

  clearAccService() {
    this._accService = null;
    Cu.forceGC();
  },

  /**
   * Adds an observer for an 'a11y-consumers-changed' event.
   */
  addAccConsumersChangedObserver() {
    const deferred = {};
    this._accConsumersChanged = new Promise(resolve => {
      deferred.resolve = resolve;
    });
    const observe = (subject, topic, data) => {
      Services.obs.removeObserver(observe, "a11y-consumers-changed");
      deferred.resolve(JSON.parse(data));
    };
    Services.obs.addObserver(observe, "a11y-consumers-changed");
  },

  /**
   * Returns a promise that resolves when 'a11y-consumers-changed' event is
   * fired.
   *
   * @return {Promise}
   *         event promise evaluating to event's data
   */
  observeAccConsumersChanged() {
    return this._accConsumersChanged;
  },

  /**
   * Adds an observer for an 'a11y-init-or-shutdown' event with a value of "1"
   * which indicates that an accessibility service is initialized in the current
   * process.
   */
  addAccServiceInitializedObserver() {
    const deferred = {};
    this._accServiceInitialized = new Promise((resolve, reject) => {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });
    const observe = (subject, topic, data) => {
      if (data === "1") {
        Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
        deferred.resolve();
      } else {
        deferred.reject("Accessibility service is shutdown unexpectedly.");
      }
    };
    Services.obs.addObserver(observe, "a11y-init-or-shutdown");
  },

  /**
   * Returns a promise that resolves when an accessibility service is
   * initialized in the current process. Otherwise (if the service is shutdown)
   * the promise is rejected.
   */
  observeAccServiceInitialized() {
    return this._accServiceInitialized;
  },

  /**
   * Adds an observer for an 'a11y-init-or-shutdown' event with a value of "0"
   * which indicates that an accessibility service is shutdown in the current
   * process.
   */
  addAccServiceShutdownObserver() {
    const deferred = {};
    this._accServiceShutdown = new Promise((resolve, reject) => {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });
    const observe = (subject, topic, data) => {
      if (data === "0") {
        Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
        deferred.resolve();
      } else {
        deferred.reject("Accessibility service is initialized unexpectedly.");
      }
    };
    Services.obs.addObserver(observe, "a11y-init-or-shutdown");
  },

  /**
   * Returns a promise that resolves when an accessibility service is shutdown
   * in the current process. Otherwise (if the service is initialized) the
   * promise is rejected.
   */
  observeAccServiceShutdown() {
    return this._accServiceShutdown;
  },

  /**
   * Extract DOMNode id from an accessible. If the accessible is in the remote
   * process, DOMNode is not present in parent process. However, if specified by
   * the author, DOMNode id will be attached to an accessible object.
   *
   * @param  {nsIAccessible} accessible  accessible
   * @return {String?}                   DOMNode id if available
   */
  getAccessibleDOMNodeID(accessible) {
    if (accessible instanceof Ci.nsIAccessibleDocument) {
      // If accessible is a document, trying to find its document body id.
      try {
        return accessible.DOMNode.body.id;
      } catch (e) {
        /* This only works if accessible is not a proxy. */
      }
    }
    try {
      return accessible.DOMNode.id;
    } catch (e) {
      /* This will fail if DOMNode is in different process. */
    }
    try {
      // When e10s is enabled, accessible will have an "id" property if its
      // corresponding DOMNode has an id. If accessible is a document, its "id"
      // property corresponds to the "id" of its body element.
      return accessible.id;
    } catch (e) {
      /* This will fail if accessible is not a proxy. */
    }

    return null;
  },

  getObjAddress(obj) {
    const exp = /native\s*@\s*(0x[a-f0-9]+)/g;
    const match = exp.exec(obj.toString());
    if (match) {
      return match[1];
    }

    return obj.toString();
  },

  getNodePrettyName(node) {
    try {
      let tag = "";
      if (node.nodeType == Node.DOCUMENT_NODE) {
        tag = "document";
      } else {
        tag = node.localName;
        if (node.nodeType == Node.ELEMENT_NODE && node.hasAttribute("id")) {
          tag += `@id="${node.getAttribute("id")}"`;
        }
      }

      return `"${tag} node", address: ${this.getObjAddress(node)}`;
    } catch (e) {
      return `" no node info "`;
    }
  },

  /**
   * Convert role to human readable string.
   */
  roleToString(role) {
    return this.accService.getStringRole(role);
  },

  /**
   * Shorten a long string if it exceeds MAX_TRIM_LENGTH.
   *
   * @param aString the string to shorten.
   *
   * @returns the shortened string.
   */
  shortenString(str) {
    if (str.length <= MAX_TRIM_LENGTH) {
      return str;
    }

    // Trim the string if its length is > MAX_TRIM_LENGTH characters.
    const trimOffset = MAX_TRIM_LENGTH / 2;

    return `${str.substring(0, trimOffset - 1)}…${str.substring(
      str.length - trimOffset,
      str.length
    )}`;
  },

  normalizeAccTreeObj(obj) {
    const key = Object.keys(obj)[0];
    const roleName = `ROLE_${key}`;
    if (roleName in Ci.nsIAccessibleRole) {
      return {
        role: Ci.nsIAccessibleRole[roleName],
        children: obj[key],
      };
    }

    return obj;
  },

  stringifyTree(obj) {
    let text = this.roleToString(obj.role) + ": [ ";
    if ("children" in obj) {
      for (let i = 0; i < obj.children.length; i++) {
        const c = this.normalizeAccTreeObj(obj.children[i]);
        text += this.stringifyTree(c);
        if (i < obj.children.length - 1) {
          text += ", ";
        }
      }
    }

    return `${text}] `;
  },

  /**
   * Return pretty name for identifier, it may be ID, DOM node or accessible.
   */
  prettyName(identifier) {
    if (identifier instanceof Array) {
      let msg = "";
      for (let idx = 0; idx < identifier.length; idx++) {
        if (msg != "") {
          msg += ", ";
        }

        msg += this.prettyName(identifier[idx]);
      }
      return msg;
    }

    if (identifier instanceof Ci.nsIAccessible) {
      const acc = this.getAccessible(identifier);
      const domID = this.getAccessibleDOMNodeID(acc);
      let msg = "[";
      try {
        if (Services.appinfo.browserTabsRemoteAutostart) {
          if (domID) {
            msg += `DOM node id: ${domID}, `;
          }
        } else {
          msg += `${this.getNodePrettyName(acc.DOMNode)}, `;
        }
        msg += `role: ${this.roleToString(acc.role)}`;
        if (acc.name) {
          msg += `, name: "${this.shortenString(acc.name)}"`;
        }
      } catch (e) {
        msg += "defunct";
      }

      if (acc) {
        msg += `, address: ${this.getObjAddress(acc)}`;
      }
      msg += "]";

      return msg;
    }

    if (Node.isInstance(identifier)) {
      return `[ ${this.getNodePrettyName(identifier)} ]`;
    }

    if (identifier && typeof identifier === "object") {
      const treeObj = this.normalizeAccTreeObj(identifier);
      if ("role" in treeObj) {
        return `{ ${this.stringifyTree(treeObj)} }`;
      }

      return JSON.stringify(identifier);
    }

    return ` "${identifier}" `;
  },

  /**
   * Return accessible for the given identifier (may be ID attribute or DOM
   * element or accessible object) or null.
   *
   * @param accOrElmOrID
   *        identifier to get an accessible implementing the given interfaces
   * @param aInterfaces
   *        [optional] the interface or an array interfaces to query it/them
   *        from obtained accessible
   * @param elmObj
   *        [optional] object to store DOM element which accessible is obtained
   *        for
   * @param doNotFailIf
   *        [optional] no error for special cases (see DONOTFAIL_IF_NO_ACC,
   *        DONOTFAIL_IF_NO_INTERFACE)
   * @param doc
   *        [optional] document for when accOrElmOrID is an ID.
   */
  getAccessible(accOrElmOrID, interfaces, elmObj, doNotFailIf, doc) {
    if (!accOrElmOrID) {
      return null;
    }

    let elm = null;
    if (accOrElmOrID instanceof Ci.nsIAccessible) {
      try {
        elm = accOrElmOrID.DOMNode;
      } catch (e) {}
    } else if (Node.isInstance(accOrElmOrID)) {
      elm = accOrElmOrID;
    } else {
      elm = doc.getElementById(accOrElmOrID);
      if (!elm) {
        Assert.ok(false, `Can't get DOM element for ${accOrElmOrID}`);
        return null;
      }
    }

    if (elmObj && typeof elmObj == "object") {
      elmObj.value = elm;
    }

    let acc = accOrElmOrID instanceof Ci.nsIAccessible ? accOrElmOrID : null;
    if (!acc) {
      try {
        acc = this.accService.getAccessibleFor(elm);
      } catch (e) {}

      if (!acc) {
        if (!(doNotFailIf & this.DONOTFAIL_IF_NO_ACC)) {
          Assert.ok(
            false,
            `Can't get accessible for ${this.prettyName(accOrElmOrID)}`
          );
        }

        return null;
      }
    }

    if (!interfaces) {
      return acc;
    }

    if (!(interfaces instanceof Array)) {
      interfaces = [interfaces];
    }

    for (let index = 0; index < interfaces.length; index++) {
      if (acc instanceof interfaces[index]) {
        continue;
      }

      try {
        acc.QueryInterface(interfaces[index]);
      } catch (e) {
        if (!(doNotFailIf & this.DONOTFAIL_IF_NO_INTERFACE)) {
          Assert.ok(
            false,
            `Can't query ${interfaces[index]} for ${accOrElmOrID}`
          );
        }

        return null;
      }
    }

    return acc;
  },

  /**
   * Return the DOM node by identifier (may be accessible, DOM node or ID).
   */
  getNode(accOrNodeOrID, doc) {
    if (!accOrNodeOrID) {
      return null;
    }

    if (Node.isInstance(accOrNodeOrID)) {
      return accOrNodeOrID;
    }

    if (accOrNodeOrID instanceof Ci.nsIAccessible) {
      return accOrNodeOrID.DOMNode;
    }

    const node = doc.getElementById(accOrNodeOrID);
    if (!node) {
      Assert.ok(false, `Can't get DOM element for ${accOrNodeOrID}`);
      return null;
    }

    return node;
  },

  /**
   * Return root accessible.
   *
   * @param  {DOMNode} doc
   *         Chrome document.
   *
   * @return {nsIAccessible}
   *         Accessible object for chrome window.
   */
  getRootAccessible(doc) {
    const acc = this.getAccessible(doc);
    return acc ? acc.rootDocument.QueryInterface(Ci.nsIAccessible) : null;
  },

  /**
   * Analogy of SimpleTest.is function used to compare objects.
   */
  isObject(obj, expectedObj, msg) {
    if (obj == expectedObj) {
      Assert.ok(true, msg);
      return;
    }

    Assert.ok(
      false,
      `${msg} - got "${this.prettyName(obj)}", expected "${this.prettyName(
        expectedObj
      )}"`
    );
  },
};
