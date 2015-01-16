/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const {AppManager} = require("devtools/webide/app-manager");
const {Connection} = require("devtools/client/connection-manager");
const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

let devicePrefsKeys = {};
let table;

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  document.getElementById("close").onclick = CloseUI;
  AppManager.on("app-manager-update", OnAppManagerUpdate);
  document.getElementById("device-preferences").onchange = UpdatePref;
  document.getElementById("device-preferences").onclick = CheckReset;
  document.getElementById("search-bar").onkeyup = document.getElementById("search-bar").onclick = SearchPref;
  document.getElementById("custom-value").onclick = UpdateNewPref;
  document.getElementById("custom-value-type").onchange = ClearNewPrefs;
  document.getElementById("add-custom-preference").onkeyup = CheckNewPrefSubmit;
  BuildUI();
}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  AppManager.off("app-manager-update", OnAppManagerUpdate);
});

function CloseUI() {
  window.parent.UI.openProject();
}

function OnAppManagerUpdate(event, what) {
  if (what == "connection" || what == "list-tabs-response") {
    BuildUI();
  }
}

function RenderByType(input, name, value, customType) {
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
    default:
      input.setAttribute("data-type", "string");
      input.setAttribute("type", "text");
      break;
  }
  return input;
}

let defaultPref; // Used by tests
function ResetToDefault(name, input, button) {
  AppManager.preferenceFront.clearUserPref(name);
  let dataType = input.getAttribute("data-type");
  let tr = document.getElementById("row-" + name);

  switch (dataType) {
    case "boolean":
      defaultPref = AppManager.preferenceFront.getBoolPref(name);
      defaultPref.then(boolean => {
        input.checked = boolean;
      }, () => {
        input.checked = false;
        tr.parentNode.removeChild(tr);
      });

      break;
    case "number":
      defaultPref = AppManager.preferenceFront.getIntPref(name);
      defaultPref.then(number => {
        input.value = number;
      }, () => {
        tr.parentNode.removeChild(tr);
      });
      break;
    default:
      defaultPref = AppManager.preferenceFront.getCharPref(name);
      defaultPref.then(string => {
        input.value = string;
      }, () => {
        tr.parentNode.removeChild(tr);
      });
      break;
  }

  button.classList.add("hide");
}

function SaveByType(options) {
  let prefName = options.id;
  let inputType = options.type;
  let value = options.value;
  let input = document.getElementById(prefName);

  switch (inputType) {
    case "boolean":
      AppManager.preferenceFront.setBoolPref(prefName, input.checked);
      break;
    case "number":
      AppManager.preferenceFront.setIntPref(prefName, value);
      break;
    default:
      AppManager.preferenceFront.setCharPref(prefName, value);
      break;
  }
}

function CheckNewPrefSubmit(event) {
  if (event.keyCode === 13) {
    document.getElementById("custom-value").click();
  }
}

function ClearNewPrefs() {
  let customTextEl = table.querySelector("#custom-value-text");
  if (customTextEl.checked) {
    customTextEl.checked = false;
  } else {
    customTextEl.value = "";
  }

  UpdateFieldType();
}

function UpdateFieldType() {
  let customValueType = table.querySelector("#custom-value-type").value;
  let customTextEl = table.querySelector("#custom-value-text");
  let customText = customTextEl.value;

  if (customValueType.length === 0) {
    return false;
  }

  switch (customValueType) {
    case "boolean":
      customTextEl.type = "checkbox";
      customText = customTextEl.checked;
      break;
    case "number":
      customText = parseInt(customText, 10) || 0;
      customTextEl.type = "number";
      break;
    default:
      customTextEl.type = "text";
      break;
  }

  return customValueType;
}

function UpdateNewPref(event) {
  let customValueType = UpdateFieldType();

  if (!customValueType) {
    return;
  }

  let customRow = table.querySelector("tr:nth-of-type(2)");
  let customTextEl = table.querySelector("#custom-value-text");
  let customTextNameEl = table.querySelector("#custom-value-name");

  if (customTextEl.validity.valid) {
    let customText = customTextEl.value;

    if (customValueType === "boolean") {
      customText = customTextEl.checked;
    }
    let customTextName = customTextNameEl.value.replace(/[^A-Za-z0-9\.\-_]/gi, "");
    GenerateField(customTextName, customText, true, customValueType, customRow);
    SaveByType({
      id: customTextName,
      type: customValueType,
      value: customText
    });
    customTextNameEl.value = "";
    ClearNewPrefs();
  }
}

function CheckReset(event) {
  if (event.target.classList.contains("reset")) {
    let btnId = event.target.getAttribute("data-id");
    let input = document.getElementById(btnId);
    ResetToDefault(btnId, input, event.target);
  }
}

function UpdatePref(event) {
  if (event.target) {
    let inputType = event.target.getAttribute("data-type");
    let inputValue = event.target.checked || event.target.value;

    if (event.target.nodeName == "input" &&
        event.target.validity.valid &&
        event.target.classList.contains("editable")) {
      let id = event.target.id;
      if (inputType == "boolean") {
        if (event.target.checked) {
          inputValue = true;
        } else {
          inputValue = false;
        }
      }
      SaveByType({
        id: id,
        type: inputType,
        value: inputValue
      });
      document.getElementById("btn-" + id).classList.remove("hide");
    }
  }
}

function GenerateField(name, value, hasUserValue, customType, newPreferenceRow) {
  if (name.length < 1) {
    return;
  }
  let sResetDefault = Strings.GetStringFromName("devicepreferences_reset_default");
  devicePrefsKeys[name] = true;
  let input = document.createElement("input");
  let tr = document.createElement("tr");
  tr.setAttribute("id", "row-" + name);
  tr.classList.add("edit-row");
  let td = document.createElement("td");
  td.classList.add("preference-name");
  td.textContent = name;
  tr.appendChild(td);
  td = document.createElement("td");
  input.classList.add("editable");
  input.setAttribute("id", name);
  input = RenderByType(input, name, value, customType);
  if (customType === "boolean" || input.type === "checkbox") {
    input.checked = value;
  } else {
    input.value = value;
  }
  td.appendChild(input);
  tr.appendChild(td);
  td = document.createElement("td");
  td.setAttribute("id", "td-" + name);

  let button = document.createElement("button");
  button.setAttribute("data-id", name);
  button.setAttribute("id", "btn-" + name);
  button.classList.add("reset");
  button.textContent = sResetDefault;
  td.appendChild(button);
  if (!hasUserValue) {
    button.classList.add("hide");
  }
  tr.appendChild(td);

  // If this is a new preference, add it to the top of the table.
  if (newPreferenceRow) {
    let existingPref = table.querySelector("#" + name);
    if (!existingPref) {
      table.insertBefore(tr, newPreferenceRow);
    } else {
      existingPref.value = value;
    }
  } else {
    table.appendChild(tr);
  }
}

function SearchPref(event) {
  if (event.target.value.length) {
    let stringMatch = new RegExp(event.target.value, "i");

    for (let key in devicePrefsKeys) {
      let row = document.getElementById("row-" + key);
      if (key.match(stringMatch)) {
        row.classList.remove("hide");
      } else if (row) {
        row.classList.add("hide");
      }
    }
  } else {
    var trs = document.getElementsByTagName("tr");

    for (let i = 0; i < trs.length; i++) {
      trs[i].classList.remove("hide");
    }
  }
}

let getAllPrefs; // Used by tests
function BuildUI() {
  table = document.querySelector("table");
  let trs = table.querySelectorAll("tr:not(#add-custom-preference)");

  for (var i = 0; i < trs.length; i++) {
    table.removeChild(trs[i]);
  }

  if (AppManager.connection &&
      AppManager.connection.status == Connection.Status.CONNECTED &&
      AppManager.preferenceFront) {
    getAllPrefs = AppManager.preferenceFront.getAllPrefs();
    getAllPrefs.then(json => {
      let devicePrefs = Object.keys(json);
      devicePrefs.sort();
      for (let i = 0; i < devicePrefs.length; i++) {
        GenerateField(devicePrefs[i], json[devicePrefs[i]].value, json[devicePrefs[i]].hasUserValue);
      }
    });
  } else {
    CloseUI();
  }
}
