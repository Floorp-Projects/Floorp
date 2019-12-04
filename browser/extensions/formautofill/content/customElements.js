/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */
/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

"use strict";

// Wrap in a block to prevent leaking to window scope.
(() => {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );

  function sendMessageToBrowser(msgName, data) {
    let { AutoCompleteParent } = ChromeUtils.import(
      "resource://gre/actors/AutoCompleteParent.jsm"
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

  MozElements.MozAutocompleteProfileListitem = class MozAutocompleteProfileListitem extends MozAutocompleteProfileListitemBase {
    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      this.textContent = "";

      this.appendChild(
        MozXULElement.parseXULToFragment(`
        <div xmlns="http://www.w3.org/1999/xhtml" class="autofill-item-box">
          <div class="profile-label-col profile-item-col">
            <span class="profile-label-affix"></span>
            <span class="profile-label"></span>
          </div>
          <div class="profile-comment-col profile-item-col">
            <span class="profile-comment"></span>
          </div>
        </div>
      `)
      );

      this._itemBox = this.querySelector(".autofill-item-box");
      this._labelAffix = this.querySelector(".profile-label-affix");
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

      return val;
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

      let { primaryAffix, primary, secondary } = JSON.parse(
        this.getAttribute("ac-value")
      );

      this._labelAffix.textContent = primaryAffix;
      this._label.textContent = primary;
      this._comment.textContent = secondary;
    }
  };

  customElements.define(
    "autocomplete-profile-listitem",
    MozElements.MozAutocompleteProfileListitem,
    { extends: "richlistitem" }
  );

  class MozAutocompleteProfileListitemFooter extends MozAutocompleteProfileListitemBase {
    constructor() {
      super();

      this.addEventListener("click", event => {
        if (event.button != 0) {
          return;
        }

        if (this._warningTextBox.contains(event.originalTarget)) {
          return;
        }

        window.openPreferences("privacy-form-autofill");
      });
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      this.textContent = "";
      this.appendChild(
        MozXULElement.parseXULToFragment(`
        <div xmlns="http://www.w3.org/1999/xhtml" class="autofill-item-box autofill-footer">
          <div class="autofill-footer-row autofill-warning"></div>
          <div class="autofill-footer-row autofill-button"></div>
        </div>
      `)
      );

      this._itemBox = this.querySelector(".autofill-footer");
      this._optionButton = this.querySelector(".autofill-button");
      this._warningTextBox = this.querySelector(".autofill-warning");

      /**
       * A handler for updating warning message once selectedIndex has been changed.
       *
       * There're three different states of warning message:
       * 1. None of addresses were selected: We show all the categories intersection of fields in the
       *    form and fields in the results.
       * 2. An address was selested: Show the additional categories that will also be filled.
       * 3. An address was selected, but the focused category is the same as the only one category: Only show
       * the exact category that we're going to fill in.
       *
       * @private
       * @param {Object} data
       *        Message data
       * @param {string[]} data.categories
       *        The categories of all the fields contained in the selected address.
       */
      this.updateWarningNote = data => {
        let categories =
          data && data.categories ? data.categories : this._allFieldCategories;
        // If the length of categories is 1, that means all the fillable fields are in the same
        // category. We will change the way to inform user according to this flag. When the value
        // is true, we show "Also autofills ...", otherwise, show "Autofills ..." only.
        let hasExtraCategories = categories.length > 1;
        // Show the categories in certain order to conform with the spec.
        let orderedCategoryList = [
          { id: "address", l10nId: "category.address" },
          { id: "name", l10nId: "category.name" },
          { id: "organization", l10nId: "category.organization2" },
          { id: "tel", l10nId: "category.tel" },
          { id: "email", l10nId: "category.email" },
        ];
        let showCategories = hasExtraCategories
          ? orderedCategoryList.filter(
              category =>
                categories.includes(category.id) &&
                category.id != this._focusedCategory
            )
          : [
              orderedCategoryList.find(
                category => category.id == this._focusedCategory
              ),
            ];

        let separator = this._stringBundle.GetStringFromName(
          "fieldNameSeparator"
        );
        let warningTextTmplKey = hasExtraCategories
          ? "phishingWarningMessage"
          : "phishingWarningMessage2";
        let categoriesText = showCategories
          .map(category =>
            this._stringBundle.GetStringFromName(category.l10nId)
          )
          .join(separator);

        this._warningTextBox.textContent = this._stringBundle.formatStringFromName(
          warningTextTmplKey,
          [categoriesText]
        );
        this.parentNode.parentNode.adjustHeight();
      };

      this._adjustAcItem();
    }

    _onCollapse() {
      if (this.showWarningText) {
        let { FormAutofillParent } = ChromeUtils.import(
          "resource://formautofill/FormAutofillParent.jsm"
        );
        FormAutofillParent.removeMessageObserver(this);
      }
      this._itemBox.removeAttribute("no-warning");
    }

    _adjustAcItem() {
      this._adjustAutofillItemLayout();
      this.setAttribute("formautofillattached", "true");

      let { AppConstants } = ChromeUtils.import(
        "resource://gre/modules/AppConstants.jsm",
        {}
      );
      // TODO: The "Short" suffix is pointless now as normal version string is no longer needed,
      // we should consider removing the suffix if possible when the next time locale change.
      let buttonTextBundleKey =
        AppConstants.platform == "macosx"
          ? "autocompleteFooterOptionOSXShort"
          : "autocompleteFooterOptionShort";
      let buttonText = this._stringBundle.GetStringFromName(
        buttonTextBundleKey
      );
      this._optionButton.textContent = buttonText;

      let value = JSON.parse(this.getAttribute("ac-value"));

      this._allFieldCategories = value.categories;
      this._focusedCategory = value.focusedCategory;
      this.showWarningText = this._allFieldCategories && this._focusedCategory;

      if (this.showWarningText) {
        let { FormAutofillParent } = ChromeUtils.import(
          "resource://formautofill/FormAutofillParent.jsm"
        );
        FormAutofillParent.addMessageObserver(this);
        this.updateWarningNote();
      } else {
        this._itemBox.setAttribute("no-warning", "true");
      }
    }
  }

  customElements.define(
    "autocomplete-profile-listitem-footer",
    MozAutocompleteProfileListitemFooter,
    { extends: "richlistitem" }
  );

  class MozAutocompleteCreditcardInsecureField extends MozAutocompleteProfileListitemBase {
    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }
      this.textContent = "";
      this.appendChild(
        MozXULElement.parseXULToFragment(`
        <div xmlns="http://www.w3.org/1999/xhtml" class="autofill-insecure-item"></div>
      `)
      );

      this._itemBox = this.querySelector(".autofill-insecure-item");

      this._adjustAcItem();
    }

    set selected(val) {
      // Make this item unselectable since we see this item as a pure message.
      return false;
    }

    get selected() {
      return this.getAttribute("selected") == "true";
    }

    _adjustAcItem() {
      this._adjustAutofillItemLayout();
      this.setAttribute("formautofillattached", "true");

      let value = this.getAttribute("ac-value");
      this._itemBox.textContent = value;
    }
  }

  customElements.define(
    "autocomplete-creditcard-insecure-field",
    MozAutocompleteCreditcardInsecureField,
    { extends: "richlistitem" }
  );

  class MozAutocompleteProfileListitemClearButton extends MozAutocompleteProfileListitemBase {
    constructor() {
      super();

      this.addEventListener("click", event => {
        if (event.button != 0) {
          return;
        }

        sendMessageToBrowser("FormAutofill:ClearForm");
      });
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      this.textContent = "";
      this.appendChild(
        MozXULElement.parseXULToFragment(`
        <div xmlns="http://www.w3.org/1999/xhtml" class="autofill-item-box autofill-footer">
          <div class="autofill-footer-row autofill-button"></div>
        </div>
      `)
      );

      this._itemBox = this.querySelector(".autofill-item-box");
      this._clearBtn = this.querySelector(".autofill-button");

      this._adjustAcItem();
    }

    _adjustAcItem() {
      this._adjustAutofillItemLayout();
      this.setAttribute("formautofillattached", "true");

      let clearFormBtnLabel = this._stringBundle.GetStringFromName(
        "clearFormBtnLabel2"
      );
      this._clearBtn.textContent = clearFormBtnLabel;
    }
  }

  customElements.define(
    "autocomplete-profile-listitem-clear-button",
    MozAutocompleteProfileListitemClearButton,
    { extends: "richlistitem" }
  );
})();
