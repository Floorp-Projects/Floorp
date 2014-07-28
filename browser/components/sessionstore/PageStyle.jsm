/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PageStyle"];

const Ci = Components.interfaces;

/**
 * The external API exported by this module.
 */
this.PageStyle = Object.freeze({
  collect: function (docShell, frameTree) {
    return PageStyleInternal.collect(docShell, frameTree);
  },

  restore: function (docShell, frameList, pageStyle) {
    PageStyleInternal.restore(docShell, frameList, pageStyle);
  },

  restoreTree: function (docShell, data) {
    PageStyleInternal.restoreTree(docShell, data);
  }
});

// Signifies that author style level is disabled for the page.
const NO_STYLE = "_nostyle";

let PageStyleInternal = {
  /**
   * Collects the selected style sheet sets for all reachable frames.
   */
  collect: function (docShell, frameTree) {
    let result = frameTree.map(({document: doc}) => {
      let style;

      if (doc) {
        // http://dev.w3.org/csswg/cssom/#persisting-the-selected-css-style-sheet-set
        style = doc.selectedStyleSheetSet || doc.lastStyleSheetSet;
      }

      return style ? {pageStyle: style} : null;
    });

    let markupDocumentViewer =
      docShell.contentViewer;

    if (markupDocumentViewer.authorStyleDisabled) {
      result = result || {};
      result.disabled = true;
    }

    return result && Object.keys(result).length ? result : null;
  },

  /**
   * Restore the selected style sheet of all the frames in frameList
   * to match |pageStyle|.
   * @param docShell the root docshell of all the frames
   * @param frameList a list of [frame, data] pairs, where frame is a
   * DOM window and data is the session restore data associated with
   * it.
   * @param pageStyle the title of the style sheet to apply
   */
  restore: function (docShell, frameList, pageStyle) {
    let disabled = pageStyle == NO_STYLE;

    let markupDocumentViewer =
      docShell.contentViewer;
    markupDocumentViewer.authorStyleDisabled = disabled;

    for (let [frame, data] of frameList) {
      Array.forEach(frame.document.styleSheets, function(aSS) {
        aSS.disabled = aSS.title && aSS.title != pageStyle;
      });
    }
  },

  /**
   * Restores pageStyle data for the current frame hierarchy starting at the
   * |docShell's| current DOMWindow using the given pageStyle |data|.
   *
   * Warning: If the current frame hierarchy doesn't match that of the given
   * |data| object we will silently discard data for unreachable frames. We may
   * as well assign page styles to the wrong frames if some were reordered or
   * removed.
   *
   * @param docShell (nsIDocShell)
   * @param data (object)
   *        {
   *          disabled: true, // when true, author styles will be disabled
   *          pageStyle: "Dusk",
   *          children: [
   *            null,
   *            {pageStyle: "Mozilla", children: [ ... ]}
   *          ]
   *        }
   */
  restoreTree: function (docShell, data) {
    let disabled = data.disabled || false;
    let markupDocumentViewer =
      docShell.contentViewer;
    markupDocumentViewer.authorStyleDisabled = disabled;

    function restoreFrame(root, data) {
      if (data.hasOwnProperty("pageStyle")) {
        root.document.selectedStyleSheetSet = data.pageStyle;
      }

      if (!data.hasOwnProperty("children")) {
        return;
      }

      let frames = root.frames;
      data.children.forEach((child, index) => {
        if (child && index < frames.length) {
          restoreFrame(frames[index], child);
        }
      });
    }

    let ifreq = docShell.QueryInterface(Ci.nsIInterfaceRequestor);
    restoreFrame(ifreq.getInterface(Ci.nsIDOMWindow), data);
  }
};
