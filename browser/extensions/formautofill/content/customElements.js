/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */
/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

"use strict";

// Wrap in a block to prevent leaking to window scope.
(() => {
  function sendMessageToBrowser(msgName, data) {
    let { AutoCompleteParent } = ChromeUtils.importESModule(
      "resource://gre/actors/AutoCompleteParent.sys.mjs"
    );

    let actor = AutoCompleteParent.getCurrentActor();
    if (!actor) {
      return;
    }

    actor.manager.getActor("FormAutofill").sendAsyncMessage(msgName, data);
  }

  class MozAutocompleteProfileListitemBase extends MozElements.MozRichlistitem {
    constructor() {
      super();

      /**
       * For form autofill, we want to unify the selection no matter by
       * keyboard navigation or mouseover in order not to confuse user which
       * profile preview is being shown. This field is set to true to indicate
       * that selectedIndex of popup should be changed while mouseover item
       */
      this.selectedByMouseOver = true;
    }

    get _stringBundle() {
      if (!this.__stringBundle) {
        this.__stringBundle = Services.strings.createBundle(
          "chrome://formautofill/locale/formautofill.properties"
        );
      }
      return this.__stringBundle;
    }

    _cleanup() {
      this.removeAttribute("formautofillattached");
      if (this._itemBox) {
        this._itemBox.removeAttribute("size");
      }
    }

    _onOverflow() {}

    _onUnderflow() {}

    handleOverUnderflow() {}

    _adjustAutofillItemLayout() {
      let outerBoxRect = this.parentNode.getBoundingClientRect();

      // Make item fit in popup as XUL box could not constrain
      // item's width
      this._itemBox.style.width = outerBoxRect.width + "px";
      // Use two-lines layout when width is smaller than 150px or
      // 185px if an image precedes the label.
      let oneLineMinRequiredWidth = this.getAttribute("ac-image") ? 185 : 150;

      if (outerBoxRect.width <= oneLineMinRequiredWidth) {
        this._itemBox.setAttribute("size", "small");
      } else {
        this._itemBox.removeAttribute("size");
      }
    }
  }

  MozElements.MozAutocompleteProfileListitem = class MozAutocompleteProfileListitem extends (
    MozAutocompleteProfileListitemBase
  ) {
    static get markup() {
      return `
        <div xmlns="http://www.w3.org/1999/xhtml" class="autofill-item-box">
          <div class="profile-label-col profile-item-col">
            <span class="profile-label"></span>
          </div>
          <div class="profile-comment-col profile-item-col">
            <span class="profile-comment"></span>
          </div>
        </div>
        `;
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      this.textContent = "";

      this.appendChild(this.constructor.fragment);

      this._itemBox = this.querySelector(".autofill-item-box");
      this._label = this.querySelector(".profile-label");
      this._comment = this.querySelector(".profile-comment");

      this.initializeAttributeInheritance();
      this._adjustAcItem();
    }

    static get inheritedAttributes() {
      return {
        ".autofill-item-box": "ac-image",
      };
    }

    set selected(val) {
      if (val) {
        this.setAttribute("selected", "true");
      } else {
        this.removeAttribute("selected");
      }

      sendMessageToBrowser("FormAutofill:PreviewProfile");
    }

    get selected() {
      return this.getAttribute("selected") == "true";
    }

    _adjustAcItem() {
      this._adjustAutofillItemLayout();
      this.setAttribute("formautofillattached", "true");
      this._itemBox.style.setProperty(
        "--primary-icon",
        `url(${this.getAttribute("ac-image")})`
      );

      let { primary, secondary, ariaLabel } = JSON.parse(
        this.getAttribute("ac-value")
      );

      this._label.textContent = primary.toString().replaceAll("*", "•");
      this._comment.textContent = secondary.toString().replaceAll("*", "•");
      if (ariaLabel) {
        this.setAttribute("aria-label", ariaLabel);
      }
    }
  };

  customElements.define(
    "autocomplete-profile-listitem",
    MozElements.MozAutocompleteProfileListitem,
    { extends: "richlistitem" }
  );
})();
