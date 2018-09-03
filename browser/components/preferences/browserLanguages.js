/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../toolkit/content/preferencesBindings.js */

ChromeUtils.import("resource://gre/modules/Services.jsm");

class OrderedListBox {
  constructor({richlistbox, upButton, downButton}) {
    this.richlistbox = richlistbox;
    this.upButton = upButton;
    this.downButton = downButton;

    this.items = [];

    this.richlistbox.addEventListener("select", () => this.setButtonState());
    this.upButton.addEventListener("command", () => this.moveUp());
    this.downButton.addEventListener("command", () => this.moveDown());
  }

  setButtonState() {
    let { upButton, downButton } = this;
    switch (this.richlistbox.selectedCount) {
    case 0:
      upButton.disabled = downButton.disabled = true;
      break;
    case 1:
      upButton.disabled = this.richlistbox.selectedIndex == 0;
      downButton.disabled = this.richlistbox.selectedIndex == this.richlistbox.childNodes.length - 1;
      break;
    default:
      upButton.disabled = true;
      downButton.disabled = true;
    }
  }

  moveUp() {
    let {selectedIndex} = this.richlistbox;
    if (selectedIndex == 0) {
      return;
    }
    let {items} = this;
    let selectedItem = items[selectedIndex];
    let prevItem = items[selectedIndex - 1];
    items[selectedIndex - 1] = items[selectedIndex];
    items[selectedIndex] = prevItem;
    let prevEl = document.getElementById(prevItem.id);
    let selectedEl = document.getElementById(selectedItem.id);
    this.richlistbox.insertBefore(selectedEl, prevEl);
    this.richlistbox.ensureElementIsVisible(selectedEl);
    this.setButtonState();
  }

  moveDown() {
    let {selectedIndex} = this.richlistbox;
    if (selectedIndex == this.items.length - 1) {
      return;
    }
    let {items} = this;
    let selectedItem = items[selectedIndex];
    let nextItem = items[selectedIndex + 1];
    items[selectedIndex + 1] = items[selectedIndex];
    items[selectedIndex] = nextItem;
    let nextEl = document.getElementById(nextItem.id);
    let selectedEl = document.getElementById(selectedItem.id);
    this.richlistbox.insertBefore(nextEl, selectedEl);
    this.richlistbox.ensureElementIsVisible(selectedEl);
    this.setButtonState();
  }

  setItems(items) {
    this.items = items;
    this.populate();
    this.setButtonState();
  }

  populate() {
    this.richlistbox.textContent = "";

    for (let {id, label, value} of this.items) {
      let listitem = document.createElement("richlistitem");
      listitem.setAttribute("value", value);
      let labelEl = document.createElement("label");
      listitem.id = id;
      labelEl.textContent = label;
      listitem.appendChild(labelEl);
      this.richlistbox.appendChild(listitem);
    }

    this.richlistbox.selectedIndex = 0;
  }
}

function getLocaleDisplayInfo(localeCodes) {
  let localeNames = Services.intl.getLocaleDisplayNames(undefined, localeCodes);
  return localeCodes.map((code, i) => {
    return {
      id: "locale-" + code,
      label: localeNames[i],
      value: code,
    };
  });
}

var gBrowserLanguagesDialog = {
  _orderedListBox: null,
  requestedLocales: null,

  beforeAccept() {
    this.requestedLocales = this._orderedListBox.items.map(item => item.value);
    return true;
  },

  onLoad() {
    this._orderedListBox = new OrderedListBox({
      richlistbox: document.getElementById("activeLocales"),
      upButton: document.getElementById("up"),
      downButton: document.getElementById("down"),
    });
    // Maintain the previously requested locales even if we cancel out.
    this.requestedLocales = window.arguments[0];
    let locales = window.arguments[0]
      || Services.locale.getRequestedLocales();
    this._orderedListBox.setItems(getLocaleDisplayInfo(locales));
  },
};
