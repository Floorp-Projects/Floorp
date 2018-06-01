/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");
const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

var ConfigView;

module.exports = ConfigView = function(window) {
  EventEmitter.decorate(this);
  this._doc = window.document;
  this._keys = [];
  return this;
};

ConfigView.prototype = {
  _renderByType: function(input, name, value, customType) {
    value = customType || typeof value;

    switch (value) {
      case "boolean":
        input.setAttribute("data-type", "boolean");
        input.setAttribute("type", "checkbox");
        break;
      case "number":
        input.setAttribute("data-type", "number");
        input.setAttribute("type", "number");
        break;
      case "object":
        input.setAttribute("data-type", "object");
        input.setAttribute("type", "text");
        break;
      default:
        input.setAttribute("data-type", "string");
        input.setAttribute("type", "text");
        break;
    }
    return input;
  },

  set front(front) {
    this._front = front;
  },

  set keys(keys) {
    this._keys = keys;
  },

  get keys() {
    return this._keys;
  },

  set kind(kind) {
    this._kind = kind;
  },

  set includeTypeName(include) {
    this._includeTypeName = include;
  },

  search: function(event) {
    if (event.target.value.length) {
      const stringMatch = new RegExp(event.target.value, "i");

      for (let i = 0; i < this._keys.length; i++) {
        const key = this._keys[i];
        const row = this._doc.getElementById("row-" + key);
        if (key.match(stringMatch)) {
          row.classList.remove("hide");
        } else if (row) {
          row.classList.add("hide");
        }
      }
    } else {
      const trs = this._doc.getElementById("device-fields").querySelectorAll("tr");

      for (let i = 0; i < trs.length; i++) {
        trs[i].classList.remove("hide");
      }
    }
  },

  generateDisplay: function(json) {
    const deviceItems = Object.keys(json);
    deviceItems.sort();
    this.keys = deviceItems;
    for (let i = 0; i < this.keys.length; i++) {
      const key = this.keys[i];
      this.generateField(key, json[key].value, json[key].hasUserValue);
    }
  },

  generateField: function(name, value, hasUserValue, customType, newRow) {
    const table = this._doc.querySelector("table");
    const sResetDefault = Strings.GetStringFromName("device_reset_default");

    if (!this._keys.includes(name)) {
      this._keys.push(name);
    }

    let input = this._doc.createElement("input");
    const tr = this._doc.createElement("tr");
    tr.setAttribute("id", "row-" + name);
    tr.classList.add("edit-row");
    let td = this._doc.createElement("td");
    td.classList.add("field-name");
    td.textContent = name;
    tr.appendChild(td);
    td = this._doc.createElement("td");
    input.classList.add("editable");
    input.setAttribute("id", name);
    input = this._renderByType(input, name, value, customType);

    if (customType === "boolean" || input.type === "checkbox") {
      input.checked = value;
    } else {
      if (typeof value === "object") {
        value = JSON.stringify(value);
      }
      input.value = value;
    }

    if (!(this._includeTypeName || isNaN(parseInt(value, 10)))) {
      input.type = "number";
    }

    td.appendChild(input);
    tr.appendChild(td);
    td = this._doc.createElement("td");
    td.setAttribute("id", "td-" + name);

    const button = this._doc.createElement("button");
    button.setAttribute("data-id", name);
    button.setAttribute("id", "btn-" + name);
    button.classList.add("reset");
    button.textContent = sResetDefault;
    td.appendChild(button);

    if (!hasUserValue) {
      button.classList.add("hide");
    }

    tr.appendChild(td);

    // If this is a new field, add it to the top of the table.
    if (newRow) {
      const existing = table.querySelector("#" + name);

      if (!existing) {
        table.insertBefore(tr, newRow);
      } else {
        existing.value = value;
      }
    } else {
      table.appendChild(tr);
    }
  },

  resetTable: function() {
    const table = this._doc.querySelector("table");
    const trs = table.querySelectorAll("tr:not(#add-custom-field)");

    for (let i = 0; i < trs.length; i++) {
      table.removeChild(trs[i]);
    }

    return table;
  },

  _getCallType: function(type, name) {
    let frontName = "get";

    if (this._includeTypeName) {
      frontName += type;
    }

    return this._front[frontName + this._kind](name);
  },

  _setCallType: function(type, name, value) {
    let frontName = "set";

    if (this._includeTypeName) {
      frontName += type;
    }

    return this._front[frontName + this._kind](name, value);
  },

  _saveByType: function(options) {
    const fieldName = options.id;
    const inputType = options.type;
    let value = options.value;
    const input = this._doc.getElementById(fieldName);

    switch (inputType) {
      case "boolean":
        this._setCallType("Bool", fieldName, input.checked);
        break;
      case "number":
        this._setCallType("Int", fieldName, value);
        break;
      case "object":
        try {
          value = JSON.parse(value);
        } catch (e) {}
        this._setCallType("Object", fieldName, value);
        break;
      default:
        this._setCallType("Char", fieldName, value);
        break;
    }
  },

  updateField: function(event) {
    if (event.target) {
      const inputType = event.target.getAttribute("data-type");
      let inputValue = event.target.checked || event.target.value;

      if (event.target.nodeName == "input" &&
          event.target.validity.valid &&
          event.target.classList.contains("editable")) {
        const id = event.target.id;
        if (inputType === "boolean") {
          if (event.target.checked) {
            inputValue = true;
          } else {
            inputValue = false;
          }
        }

        this._saveByType({
          id: id,
          type: inputType,
          value: inputValue
        });
        this._doc.getElementById("btn-" + id).classList.remove("hide");
      }
    }
  },

  _resetToDefault: function(name, input, button) {
    this._front["clearUser" + this._kind](name);
    const dataType = input.getAttribute("data-type");
    const tr = this._doc.getElementById("row-" + name);

    switch (dataType) {
      case "boolean":
        this._defaultField = this._getCallType("Bool", name);
        this._defaultField.then(boolean => {
          input.checked = boolean;
        }, () => {
          input.checked = false;
          tr.remove();
        });
        break;
      case "number":
        this._defaultField = this._getCallType("Int", name);
        this._defaultField.then(number => {
          input.value = number;
        }, () => {
          tr.remove();
        });
        break;
      case "object":
        this._defaultField = this._getCallType("Object", name);
        this._defaultField.then(object => {
          input.value = JSON.stringify(object);
        }, () => {
          tr.remove();
        });
        break;
      default:
        this._defaultField = this._getCallType("Char", name);
        this._defaultField.then(string => {
          input.value = string;
        }, () => {
          tr.remove();
        });
        break;
    }

    button.classList.add("hide");
  },

  checkReset: function(event) {
    if (event.target.classList.contains("reset")) {
      const btnId = event.target.getAttribute("data-id");
      const input = this._doc.getElementById(btnId);
      this._resetToDefault(btnId, input, event.target);
    }
  },

  updateFieldType: function() {
    const table = this._doc.querySelector("table");
    const customValueType = table.querySelector("#custom-value-type").value;
    const customTextEl = table.querySelector("#custom-value-text");

    if (customValueType.length === 0) {
      return false;
    }

    switch (customValueType) {
      case "boolean":
        customTextEl.type = "checkbox";
        break;
      case "number":
        customTextEl.type = "number";
        break;
      default:
        customTextEl.type = "text";
        break;
    }

    return customValueType;
  },

  clearNewFields: function() {
    const table = this._doc.querySelector("table");
    const customTextEl = table.querySelector("#custom-value-text");
    if (customTextEl.checked) {
      customTextEl.checked = false;
    } else {
      customTextEl.value = "";
    }

    this.updateFieldType();
  },

  updateNewField: function() {
    const table = this._doc.querySelector("table");
    const customValueType = this.updateFieldType();

    if (!customValueType) {
      return;
    }

    const customRow = table.querySelector("tr:nth-of-type(2)");
    const customTextEl = table.querySelector("#custom-value-text");
    const customTextNameEl = table.querySelector("#custom-value-name");

    if (customTextEl.validity.valid) {
      let customText = customTextEl.value;

      if (customValueType === "boolean") {
        customText = customTextEl.checked;
      }

      const customTextName = customTextNameEl.value.replace(/[^A-Za-z0-9\.\-_]/gi, "");
      this.generateField(customTextName, customText, true, customValueType, customRow);
      this._saveByType({
        id: customTextName,
        type: customValueType,
        value: customText
      });
      customTextNameEl.value = "";
      this.clearNewFields();
    }
  },

  checkNewFieldSubmit: function(event) {
    if (event.keyCode === 13) {
      this._doc.getElementById("custom-value").click();
    }
  }
};
