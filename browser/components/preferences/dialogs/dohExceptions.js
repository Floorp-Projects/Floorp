/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

var gDoHExceptionsManager = {
  _exceptions: new Set(),
  _list: null,
  _prefLocked: false,

  init() {
    document.addEventListener("dialogaccept", () => this.onApplyChanges());

    this._btnAddException = document.getElementById("btnAddException");
    this._removeButton = document.getElementById("removeException");
    this._removeAllButton = document.getElementById("removeAllExceptions");
    this._list = document.getElementById("permissionsBox");

    this._urlField = document.getElementById("url");
    this.onExceptionInput();

    this._loadExceptions();
    this.buildExceptionList();

    this._urlField.focus();

    this._prefLocked = Services.prefs.prefIsLocked(
      "network.trr.excluded-domains"
    );

    this._btnAddException.disabled = this._prefLocked;
    document.getElementById("exceptionDialog").getButton("accept").disabled =
      this._prefLocked;
    this._urlField.disabled = this._prefLocked;
  },

  _loadExceptions() {
    let exceptionsFromPref = Services.prefs.getStringPref(
      "network.trr.excluded-domains"
    );

    if (!exceptionsFromPref?.trim()) {
      return;
    }

    let exceptions = exceptionsFromPref.trim().split(",");
    for (let exception of exceptions) {
      let trimmed = exception.trim();
      if (trimmed) {
        this._exceptions.add(trimmed);
      }
    }
  },

  addException() {
    if (this._prefLocked) {
      return;
    }

    let textbox = document.getElementById("url");
    let inputValue = textbox.value.trim(); // trim any leading and trailing space
    if (!inputValue.startsWith("http:") && !inputValue.startsWith("https:")) {
      inputValue = `http://${inputValue}`;
    }
    let domain = "";
    try {
      let uri = Services.io.newURI(inputValue);
      domain = uri.host;
    } catch (ex) {
      document.l10n
        .formatValues([
          { id: "permissions-invalid-uri-title" },
          { id: "permissions-invalid-uri-label" },
        ])
        .then(([title, message]) => {
          Services.prompt.alert(window, title, message);
        });
      return;
    }

    if (!this._exceptions.has(domain)) {
      this._exceptions.add(domain);
      this.buildExceptionList();
    }

    textbox.value = "";
    textbox.focus();

    // covers a case where the site exists already, so the buttons don't disable
    this.onExceptionInput();

    // enable "remove all" button as needed
    this._setRemoveButtonState();
  },

  onExceptionInput() {
    this._btnAddException.disabled = !this._urlField.value;
  },

  onExceptionKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_RETURN) {
      this._btnAddException.click();
      if (document.activeElement == this._urlField) {
        event.preventDefault();
      }
    }
  },

  onListBoxKeyPress(event) {
    if (!this._list.selectedItem) {
      return;
    }

    if (this._prefLocked) {
      return;
    }

    if (
      event.keyCode == KeyEvent.DOM_VK_DELETE ||
      (AppConstants.platform == "macosx" &&
        event.keyCode == KeyEvent.DOM_VK_BACK_SPACE)
    ) {
      this.onExceptionDelete();
      event.preventDefault();
    }
  },

  onListBoxSelect() {
    this._setRemoveButtonState();
  },

  _removeExceptionFromList(exception) {
    this._exceptions.delete(exception);
    let exceptionlistitem = document.getElementsByAttribute(
      "domain",
      exception
    )[0];
    if (exceptionlistitem) {
      exceptionlistitem.remove();
    }
  },

  onExceptionDelete() {
    let richlistitem = this._list.selectedItem;
    let exception = richlistitem.getAttribute("domain");

    this._removeExceptionFromList(exception);

    this._setRemoveButtonState();
  },

  onAllExceptionsDelete() {
    for (let exception of this._exceptions.values()) {
      this._removeExceptionFromList(exception);
    }

    this._setRemoveButtonState();
  },

  _createExceptionListItem(exception) {
    let richlistitem = document.createXULElement("richlistitem");
    richlistitem.setAttribute("domain", exception);
    let row = document.createXULElement("hbox");
    row.setAttribute("style", "flex: 1");

    let hbox = document.createXULElement("hbox");
    let website = document.createXULElement("label");
    website.setAttribute("class", "website-name-value");
    website.setAttribute("value", exception);
    hbox.setAttribute("class", "website-name");
    hbox.setAttribute("style", "flex: 3 3; width: 0");
    hbox.appendChild(website);
    row.appendChild(hbox);

    richlistitem.appendChild(row);
    return richlistitem;
  },

  _sortExceptions(list, frag, column) {
    let sortDirection;

    if (!column) {
      column = document.querySelector("treecol[data-isCurrentSortCol=true]");
      sortDirection =
        column.getAttribute("data-last-sortDirection") || "ascending";
    } else {
      sortDirection = column.getAttribute("data-last-sortDirection");
      sortDirection =
        sortDirection === "ascending" ? "descending" : "ascending";
    }

    let sortFunc = (a, b) => {
      return comp.compare(a.getAttribute("domain"), b.getAttribute("domain"));
    };

    let comp = new Services.intl.Collator(undefined, {
      usage: "sort",
    });

    let items = Array.from(frag.querySelectorAll("richlistitem"));

    if (sortDirection === "descending") {
      items.sort((a, b) => sortFunc(b, a));
    } else {
      items.sort(sortFunc);
    }

    // Re-append items in the correct order:
    items.forEach(item => frag.appendChild(item));

    let cols = list.previousElementSibling.querySelectorAll("treecol");
    cols.forEach(c => {
      c.removeAttribute("data-isCurrentSortCol");
      c.removeAttribute("sortDirection");
    });
    column.setAttribute("data-isCurrentSortCol", "true");
    column.setAttribute("sortDirection", sortDirection);
    column.setAttribute("data-last-sortDirection", sortDirection);
  },

  _setRemoveButtonState() {
    if (!this._list) {
      return;
    }

    if (this._prefLocked) {
      this._removeAllButton.disabled = true;
      this._removeButton.disabled = true;
      return;
    }

    let hasSelection = this._list.selectedIndex >= 0;

    this._removeButton.disabled = !hasSelection;
    let disabledItems = this._list.querySelectorAll(
      "label.website-name-value[disabled='true']"
    );

    this._removeAllButton.disabled =
      this._list.itemCount == disabledItems.length;
  },

  onApplyChanges() {
    if (this._exceptions.size == 0) {
      Services.prefs.setStringPref("network.trr.excluded-domains", "");
      return;
    }

    let exceptions = Array.from(this._exceptions);
    let exceptionPrefString = exceptions.join(",");

    Services.prefs.setStringPref(
      "network.trr.excluded-domains",
      exceptionPrefString
    );
  },

  buildExceptionList(sortCol) {
    // Clear old entries.
    let oldItems = this._list.querySelectorAll("richlistitem");
    for (let item of oldItems) {
      item.remove();
    }
    let frag = document.createDocumentFragment();

    let exceptions = Array.from(this._exceptions.values());

    for (let exception of exceptions) {
      let richlistitem = this._createExceptionListItem(exception);
      frag.appendChild(richlistitem);
    }

    // Sort exceptions.
    this._sortExceptions(this._list, frag, sortCol);

    this._list.appendChild(frag);

    this._setRemoveButtonState();
  },
};

document.addEventListener("DOMContentLoaded", () => {
  gDoHExceptionsManager.init();
});
