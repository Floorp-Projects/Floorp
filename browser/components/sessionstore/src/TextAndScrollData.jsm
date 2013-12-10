/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TextAndScrollData"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DocumentUtils",
  "resource:///modules/sessionstore/DocumentUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivacyLevel",
  "resource:///modules/sessionstore/PrivacyLevel.jsm");

/**
 * The external API exported by this module.
 */
this.TextAndScrollData = Object.freeze({
  updateFrame: function (entry, content, isPinned, options) {
    return TextAndScrollDataInternal.updateFrame(entry, content, isPinned, options);
  },

  restore: function (frameList) {
    TextAndScrollDataInternal.restore(frameList);
  },
});

let TextAndScrollDataInternal = {
  /**
   * Go through all subframes and store all form data, the current
   * scroll positions and innerHTML content of WYSIWYG editors.
   *
   * @param entry
   *        the object into which to store the collected data
   * @param content
   *        frame reference
   * @param isPinned
   *        the tab is pinned and should be treated differently for privacy
   * @param includePrivateData
   *        {includePrivateData:true} include privacy sensitive data (use with care)
   */
  updateFrame: function (entry, content, isPinned, options = null) {
    let includePrivateData = options && options.includePrivateData;

    for (let i = 0; i < content.frames.length; i++) {
      if (entry.children && entry.children[i]) {
        this.updateFrame(entry.children[i], content.frames[i], includePrivateData, isPinned);
      }
    }

    let href = (content.parent || content).document.location.href;
    let isHttps = Services.io.newURI(href, null, null).schemeIs("https");
    let topURL = content.top.document.location.href;
    let isAboutSR = this.isAboutSessionRestore(topURL);
    if (includePrivateData || isAboutSR ||
        PrivacyLevel.canSave({isHttps: isHttps, isPinned: isPinned})) {
      let formData = DocumentUtils.getFormData(content.document);

      // We want to avoid saving data for about:sessionrestore as a string.
      // Since it's stored in the form as stringified JSON, stringifying further
      // causes an explosion of escape characters. cf. bug 467409
      if (formData && isAboutSR) {
        formData.id["sessionData"] = JSON.parse(formData.id["sessionData"]);
      }

      if (Object.keys(formData.id).length ||
          Object.keys(formData.xpath).length) {
        entry.formdata = formData;
      }

      // designMode is undefined e.g. for XUL documents (as about:config)
      if ((content.document.designMode || "") == "on" && content.document.body) {
        entry.innerHTML = content.document.body.innerHTML;
      }
    }

    // get scroll position from nsIDOMWindowUtils, since it allows avoiding a
    // flush of layout
    let domWindowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils);
    let scrollX = {}, scrollY = {};
    domWindowUtils.getScrollXY(false, scrollX, scrollY);
    entry.scroll = scrollX.value + "," + scrollY.value;
  },

  isAboutSessionRestore: function (url) {
    return url == "about:sessionrestore" || url == "about:welcomeback";
  },

  restore: function (frameList) {
    for (let [frame, data] of frameList) {
      this.restoreFrame(frame, data);
    }
  },

  restoreFrame: function (content, data) {
    if (data.formdata) {
      let formdata = data.formdata;

      // handle backwards compatibility
      // this is a migration from pre-firefox 15. cf. bug 742051
      if (!("xpath" in formdata || "id" in formdata)) {
        formdata = { xpath: {}, id: {} };

        for each (let [key, value] in Iterator(data.formdata)) {
          if (key.charAt(0) == "#") {
            formdata.id[key.slice(1)] = value;
          } else {
            formdata.xpath[key] = value;
          }
        }
      }

      // for about:sessionrestore we saved the field as JSON to avoid
      // nested instances causing humongous sessionstore.js files.
      // cf. bug 467409
      if (this.isAboutSessionRestore(data.url) &&
          "sessionData" in formdata.id &&
          typeof formdata.id["sessionData"] == "object") {
        formdata.id["sessionData"] = JSON.stringify(formdata.id["sessionData"]);
      }

      // update the formdata
      data.formdata = formdata;
      // merge the formdata
      DocumentUtils.mergeFormData(content.document, formdata);
    }

    if (data.innerHTML) {
      // We know that the URL matches data.url right now, but the user
      // may navigate away before the setTimeout handler runs. We do
      // a simple comparison against savedURL to check for that.
      let savedURL = content.document.location.href;

      setTimeout(function() {
        if (content.document.designMode == "on" &&
            content.document.location.href == savedURL &&
            content.document.body) {
          content.document.body.innerHTML = data.innerHTML;
        }
      }, 0);
    }

    let match;
    if (data.scroll && (match = /(\d+),(\d+)/.exec(data.scroll)) != null) {
      content.scrollTo(match[1], match[2]);
    }
  },
};
