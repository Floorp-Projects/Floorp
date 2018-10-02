/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../toolkit/content/preferencesBindings.js */

ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "AddonRepository",
                               "resource://gre/modules/addons/AddonRepository.jsm");

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
  constructor({menulist, button, onSelect, onChange}) {
    this.menulist = menulist;
    this.popup = menulist.firstElementChild;
    this.button = button;
    this.items = [];

    menulist.addEventListener("command", () => {
      button.disabled = !menulist.selectedItem;
      if (menulist.selectedItem) {
        onChange(this.items[menulist.selectedIndex]);
      }
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
    let {button, items, menulist, popup} = this;
    popup.textContent = "";

    let frag = document.createDocumentFragment();
    for (let item of items) {
      frag.appendChild(this.createItem(item));
    }
    popup.appendChild(frag);

    menulist.setAttribute("label", menulist.getAttribute("placeholder"));
    menulist.disabled = menulist.itemCount == 0;
    menulist.selectedIndex = -1;
    button.disabled = true;
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

  /**
   * Disable the inputs and set a data-l10n-id on the menulist. This can be
   * reverted with `enableWithMessageId()`.
   */
  disableWithMessageId(messageId) {
    this.menulist.setAttribute("data-l10n-id", messageId);
    this.menulist.setAttribute("image", "chrome://browser/skin/tabbrowser/tab-connecting.png");
    this.menulist.disabled = true;
    this.button.disabled = true;
  }

  /**
   * Enable the inputs and set a data-l10n-id on the menulist. This can be
   * reverted with `disableWithMessageId()`.
   */
  enableWithMessageId(messageId) {
    this.menulist.setAttribute("data-l10n-id", messageId);
    this.menulist.removeAttribute("image");
    this.menulist.disabled = this.menulist.itemCount == 0;
    this.button.disabled = !this.menulist.selectedItem;
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

  async onLoad() {
    // Maintain the previously requested locales even if we cancel out.
    let {requesting, search} = window.arguments[0] || {};
    this.requestedLocales = requesting;

    let requested = this.requestedLocales || Services.locale.requestedLocales;
    let requestedSet = new Set(requested);
    let available = Services.locale.availableLocales
      .filter(locale => !requestedSet.has(locale));

    this.initRequestedLocales(requested);
    await this.initAvailableLocales(available, search);
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

  async initAvailableLocales(available, search) {
    this._availableLocales = new SortedItemSelectList({
      menulist: document.getElementById("availableLocales"),
      button: document.getElementById("add"),
      onSelect: (item) => this.availableLanguageSelected(item),
      onChange: (item) => {
        this.hideError();
        if (item.value == "search") {
          this.loadLocalesFromAMO();
        }
      },
    });

    // Populate the list with the installed locales even if the user is
    // searching in case the download fails.
    await this.loadLocalesFromInstalled(available);

    // If the user opened this from the "Search for more languages" option,
    // search AMO for available locales.
    if (search) {
      return this.loadLocalesFromAMO();
    }

    return undefined;
  },

  async loadLocalesFromAMO() {
    // Disable the dropdown while we hit the network.
    this._availableLocales.disableWithMessageId("browser-languages-searching");

    // Fetch the available langpacks from AMO.
    let availableLangpacks;
    try {
      availableLangpacks = await AddonRepository.getAvailableLangpacks();
    } catch (e) {
      this.showError();
      return;
    }

    // Store the available langpack info for later use.
    this.availableLangpacks = new Map();
    for (let {target_locale, url, hash} of availableLangpacks) {
      this.availableLangpacks.set(target_locale, {url, hash});
    }

    // Create a list of installed locales to hide.
    let installedLocales = new Set([
      ...Services.locale.requestedLocales,
      ...Services.locale.availableLocales,
    ]);

    let availableLocales = availableLangpacks
      .filter(({target_locale}) => !installedLocales.has(target_locale))
      .map(lang => lang.target_locale);
    let availableItems = getLocaleDisplayInfo(availableLocales);
    let items = this._availableLocales.items;
    // Drop the search item.
    items.pop();
    items = items.concat(availableItems);

    // Update the dropdown and enable it again.
    this._availableLocales.setItems(items);
    this._availableLocales.enableWithMessageId("browser-languages-select-language");
  },

  async loadLocalesFromInstalled(available) {
    let items;
    if (available.length > 0) {
      items = getLocaleDisplayInfo(available);
    } else {
      items = [];
    }
    items.push({
      label: await document.l10n.formatValue("browser-languages-search"),
      value: "search",
    });
    this._availableLocales.setItems(items);
  },

  async availableLanguageSelected(item) {
    let available = new Set(Services.locale.availableLocales);

    if (available.has(item.value)) {
      this._requestedLocales.addItem(item);
      if (available.size == this._requestedLocales.items.length) {
        this._availableLocales.setItems(this._availableLocales.items);
      }
    } else if (this.availableLangpacks.has(item.value)) {
      this._availableLocales.disableWithMessageId("browser-languages-downloading");

      let {url, hash} = this.availableLangpacks.get(item.value);
      let install = await AddonManager.getInstallForURL(
        url, "application/x-xpinstall", hash);

      try {
        await install.install();
      } catch (e) {
        this.showError();
        return;
      }

      item.installed = true;
      this._requestedLocales.addItem(item);
      this._availableLocales.enableWithMessageId("browser-languages-select-language");
    } else {
      this.showError();
    }
  },

  showError() {
    document.querySelectorAll(".warning-message-separator")
      .forEach(separator => separator.classList.add("thin"));
    document.getElementById("warning-message").hidden = false;
    this._availableLocales.enableWithMessageId("browser-languages-select-language");
  },

  hideError() {
    document.querySelectorAll(".warning-message-separator")
      .forEach(separator => separator.classList.remove("thin"));
    document.getElementById("warning-message").hidden = true;
  },
};
