/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../toolkit/content/preferencesBindings.js */

ChromeUtils.import("resource://gre/modules/Services.jsm");

class OrderedListBox {
  constructor({richlistbox, upButton, downButton, removeButton, onRemove}) {
    this.richlistbox = richlistbox;
    this.upButton = upButton;
    this.downButton = downButton;
    this.removeButton = removeButton;
    this.onRemove = onRemove;

    this.items = [];

    this.richlistbox.addEventListener("select", () => this.setButtonState());
    this.upButton.addEventListener("command", () => this.moveUp());
    this.downButton.addEventListener("command", () => this.moveDown());
    this.removeButton.addEventListener("command", () => this.removeItem());
  }

  get selectedItem() {
    return this.items[this.richlistbox.selectedIndex];
  }

  setButtonState() {
    let {upButton, downButton, removeButton} = this;
    let {selectedIndex, itemCount} = this.richlistbox;
    upButton.disabled = selectedIndex == 0;
    downButton.disabled = selectedIndex == itemCount - 1;
    removeButton.disabled = itemCount == 1 || !this.selectedItem.canRemove;
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

  removeItem() {
    let {selectedIndex} = this.richlistbox;

    if (selectedIndex == -1) {
      return;
    }

    let [item] = this.items.splice(selectedIndex, 1);
    this.richlistbox.selectedItem.remove();
    this.richlistbox.selectedIndex = Math.min(
      selectedIndex, this.richlistbox.itemCount - 1);
    this.richlistbox.ensureElementIsVisible(this.richlistbox.selectedItem);
    this.onRemove(item);
  }

  setItems(items) {
    this.items = items;
    this.populate();
    this.setButtonState();
  }

  /**
   * Add an item to the top of the ordered list.
   *
   * @param {object} item The item to insert.
   */
  addItem(item) {
    this.items.unshift(item);
    this.richlistbox.insertBefore(
      this.createItem(item),
      this.richlistbox.firstElementChild);
    this.richlistbox.selectedIndex = 0;
    this.richlistbox.ensureElementIsVisible(this.richlistbox.selectedItem);
  }

  populate() {
    this.richlistbox.textContent = "";

    let frag = document.createDocumentFragment();
    for (let item of this.items) {
      frag.appendChild(this.createItem(item));
    }
    this.richlistbox.appendChild(frag);

    this.richlistbox.selectedIndex = 0;
    this.richlistbox.ensureElementIsVisible(this.richlistbox.selectedItem);
  }

  createItem({id, label, value}) {
    let listitem = document.createElement("richlistitem");
    listitem.id = id;
    listitem.setAttribute("value", value);

    let labelEl = document.createElement("label");
    labelEl.textContent = label;
    listitem.appendChild(labelEl);

    return listitem;
  }
}

class SortedItemSelectList {
  constructor({menulist, button, onSelect}) {
    this.menulist = menulist;
    this.popup = menulist.firstElementChild;
    this.items = [];

    menulist.addEventListener("command", () => {
      button.disabled = !menulist.selectedItem;
    });
    button.addEventListener("command", () => {
      if (!menulist.selectedItem) return;

      let [item] = this.items.splice(menulist.selectedIndex, 1);
      menulist.selectedItem.remove();
      menulist.setAttribute("label", menulist.getAttribute("placeholder"));
      button.disabled = true;
      menulist.disabled = menulist.itemCount == 0;

      onSelect(item);
    });
  }

  setItems(items) {
    this.items = items.sort((a, b) => a.label > b.label);
    this.populate();
  }

  populate() {
    let {items, menulist, popup} = this;
    popup.textContent = "";

    let frag = document.createDocumentFragment();
    for (let item of items) {
      frag.appendChild(this.createItem(item));
    }
    popup.appendChild(frag);

    menulist.setAttribute("label", menulist.getAttribute("placeholder"));
    menulist.disabled = menulist.itemCount == 0;
  }

  /**
   * Add an item to the list sorted by the label.
   *
   * @param {object} item The item to insert.
   */
  addItem(item) {
    let {items, menulist, popup} = this;
    let i;

    // Find the index of the item to insert before.
    for (i = 0; i < items.length && items[i].label < item.label; i++)
      ;

    items.splice(i, 0, item);
    popup.insertBefore(this.createItem(item), menulist.getItemAtIndex(i));
    menulist.disabled = menulist.itemCount == 0;
  }

  createItem({label, value}) {
    let item = document.createElement("menuitem");
    item.value = value;
    item.setAttribute("label", label);
    return item;
  }
}

function getLocaleDisplayInfo(localeCodes) {
  let packagedLocales = new Set(Services.locale.packagedLocales);
  let localeNames = Services.intl.getLocaleDisplayNames(undefined, localeCodes);
  return localeCodes.map((code, i) => {
    return {
      id: "locale-" + code,
      label: localeNames[i],
      value: code,
      canRemove: !packagedLocales.has(code),
    };
  });
}

var gBrowserLanguagesDialog = {
  _availableLocales: null,
  _requestedLocales: null,
  requestedLocales: null,

  beforeAccept() {
    this.requestedLocales = this._requestedLocales.items.map(item => item.value);
    return true;
  },

  onLoad() {
    // Maintain the previously requested locales even if we cancel out.
    this.requestedLocales = window.arguments[0];

    let requested = this.requestedLocales || Services.locale.requestedLocales;
    let requestedSet = new Set(requested);
    let available = Services.locale.availableLocales
      .filter(locale => !requestedSet.has(locale));

    this.initRequestedLocales(requested);
    this.initAvailableLocales(available);
    this.initialized = true;
  },

  initRequestedLocales(requested) {
    this._requestedLocales = new OrderedListBox({
      richlistbox: document.getElementById("requestedLocales"),
      upButton: document.getElementById("up"),
      downButton: document.getElementById("down"),
      removeButton: document.getElementById("remove"),
      onRemove: (item) => this._availableLocales.addItem(item),
    });
    this._requestedLocales.setItems(getLocaleDisplayInfo(requested));
  },

  initAvailableLocales(available) {
    this._availableLocales = new SortedItemSelectList({
      menulist: document.getElementById("availableLocales"),
      button: document.getElementById("add"),
      onSelect: (item) => this._requestedLocales.addItem(item),
    });
    this._availableLocales.setItems(getLocaleDisplayInfo(available));
  },
};
