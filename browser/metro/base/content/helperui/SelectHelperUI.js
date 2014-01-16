/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * SelectHelperUI: Provides an interface for making a choice in a list.
 *   Supports simultaneous selection of choices and group headers.
 */
var SelectHelperUI = {
  _list: null,

  get _container() {
    delete this._container;
    return this._container = document.getElementById("select-container");
  },

  get _listbox() {
    delete this._listbox;
    return this._listbox = document.getElementById("select-commands");
  },

  get _menuPopup() {
    let popup = document.getElementById("select-popup");
    delete this._menuPopup;
    return this._menuPopup = new MenuPopup(this._container, popup);
  },

  show: function selectHelperShow(aList, aTitle, aRect) {
    if (this._list) {
      this.reset();
    }

    this._list = aList;

    this._listbox.setAttribute("seltype", aList.multiple ? "multiple" : "single");

    let firstSelected = null;

    // Using a fragment prevent us to hang on huge list
    let fragment = document.createDocumentFragment();
    let choices = aList.choices;
    let selectedItems = [];
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let item = document.createElement("richlistitem");
      let label = document.createElement("label");

      item.setAttribute("class", "option-command listitem-iconic");
      item.setAttribute("flex", "1");
      item.setAttribute("crop", "center");
      label.setAttribute("value", choice.text);
      item.appendChild(label);
      item.setAttribute("oldstate", "false");
      choice.disabled ? item.setAttribute("disabled", "true")
                      : item.removeAttribute("disabled");

      fragment.appendChild(item);

      if (choice.group) {
        item.classList.add("optgroup");
        continue;
      }

      item.optionIndex = choice.optionIndex;
      item.choiceIndex = i;

      if (choice.inGroup) {
        item.classList.add("in-optgroup");
      }

      if (choice.selected) {
        firstSelected = firstSelected || item;
        selectedItems.push(item);
      }
    }
    this._listbox.appendChild(fragment);

    this._container.addEventListener("click", this, false);
    window.addEventListener("MozPrecisePointer", this, false);
    this._menuPopup.show(this._positionOptions(aRect));

    // Setup pre-selected items. Note, this has to happen after show.
    this._listbox.clearSelection();
    for (let item of selectedItems) {
      this._listbox.addItemToSelection(item);
      item.setAttribute("oldstate", "true");
    }
    this._listbox.ensureElementIsVisible(firstSelected);
  },

  reset: function selectHelperReset() {
    this._updateControl();
    while (this._listbox.hasChildNodes())
      this._listbox.removeChild(this._listbox.lastChild);
    this._list = null;
  },

  hide: function selectHelperHide() {
    if (!this._list)
      return;

    this._container.removeEventListener("click", this, false);
    window.removeEventListener("MozPrecisePointer", this, false);
    this._menuPopup.hide();
    this.reset();
  },

  _positionOptions: function _positionOptions(aRect) {
    let browser = Browser.selectedBrowser;
    let p0 = browser.ptBrowserToClient(aRect.left, aRect.top);
    let p1 = browser.ptBrowserToClient(aRect.right, aRect.bottom);

    return {
      xPos: p0.x,
      yPos: p1.y,
      bottomAligned: false,
      leftAligned: true
    };
  },

  _updateControl: function _selectHelperUpdateControl() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceChange", { });
  },

  handleEvent: function selectHelperHandleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozPrecisePointer":
        this.hide();
      break;
      case "click":
        let item = aEvent.target;
        if (item && item.hasOwnProperty("optionIndex")) {
          if (this._list.multiple) {
            // item.selected is always true here since that's how richlistbox handles
            // mouse click events. We track our own state so that we can toggle here
            // on click events. Iniial 'oldstate' values are setup in show above.
            if (item.getAttribute("oldstate") == "true") {
              item.setAttribute("oldstate", "false");
            } else {
              item.setAttribute("oldstate", "true");
            }
            // Fix up selected items - richlistbox will clear selection on click events
            // so we need to set selection on the items the user has previously chosen.
            this._listbox.clearSelection();
            for (let node of this._listbox.childNodes) {
              if (node.getAttribute("oldstate") == "true") {
                this._listbox.addItemToSelection(node);
              }
            }
          }
          // Let the form element know we've added or removed a selected item.
          this.onSelect(item.optionIndex, item.selected);
        }
        break;
    }
  },

  onSelect: function selectHelperOnSelect(aIndex, aSelected) {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceSelect", {
      index: aIndex,
      selected: aSelected
    });
    if (!this._list.multiple) {
      this.hide();
    }
  }
};
