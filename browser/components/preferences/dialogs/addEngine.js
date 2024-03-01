/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../main.js */

let gAddEngineDialog = {
  _form: null,
  _name: null,
  _alias: null,

  onLoad() {
    document.mozSubdialogReady = this.init();
  },

  async init() {
    this._dialog = document.querySelector("dialog");
    this._form = document.getElementById("addEngineForm");
    this._name = document.getElementById("engineName");
    this._alias = document.getElementById("engineAlias");

    this._name.addEventListener("input", this.onNameInput.bind(this));
    this._alias.addEventListener("input", this.onAliasInput.bind(this));
    this._form.addEventListener("input", this.onFormInput.bind(this));

    document.addEventListener("dialogaccept", this.onAddEngine.bind(this));
  },

  async onAddEngine(event) {
    let url = document
      .getElementById("engineUrl")
      .value.replace(/%s/, "{searchTerms}");
    await Services.search.wrappedJSObject.addUserEngine(
      this._name.value,
      url,
      this._alias.value
    );
  },

  async onNameInput() {
    if (this._name.value) {
      let engine = Services.search.getEngineByName(this._name.value);
      let validity = engine
        ? document.getElementById("engineNameExists").textContent
        : "";
      this._name.setCustomValidity(validity);
    }
  },

  async onAliasInput() {
    let validity = "";
    if (this._alias.value) {
      let engine = await Services.search.getEngineByAlias(this._alias.value);
      if (engine) {
        engine = document.getElementById("engineAliasExists").textContent;
      }
    }
    this._alias.setCustomValidity(validity);
  },

  async onFormInput() {
    this._dialog.setAttribute(
      "buttondisabledaccept",
      !this._form.checkValidity()
    );
  },
};

window.addEventListener("load", () => gAddEngineDialog.onLoad());
