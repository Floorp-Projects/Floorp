"use strict";var _slicedToArray = function () {function sliceIterator(arr, i) {var _arr = [];var _n = true;var _d = false;var _e = undefined;try {for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) {_arr.push(_s.value);if (i && _arr.length === i) break;}} catch (err) {_d = true;_e = err;} finally {try {if (!_n && _i["return"]) _i["return"]();} finally {if (_d) throw _e;}}return _arr;}return function (arr, i) {if (Array.isArray(arr)) {return arr;} else if (Symbol.iterator in Object(arr)) {return sliceIterator(arr, i);} else {throw new TypeError("Invalid attempt to destructure non-iterable instance");}};}(); /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       * License, v. 2.0. If a copy of the MPL was not distributed with this file,
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Components */var _Components = 
Components;var Cu = _Components.utils;

var loop = loop || {};
loop.copy = function (mozL10n) {
  "use strict";

  /**
   * Copy panel view.
   */
  var CopyView = React.createClass({ displayName: "CopyView", 
    mixins: [React.addons.PureRenderMixin], 

    getInitialState: function getInitialState() {
      return { checked: false };}, 


    /**
     * Send a message to chrome/bootstrap to handle copy panel events.
     * @param {Boolean} accept True if the user clicked accept.
     */
    _dispatch: function _dispatch(accept) {
      window.dispatchEvent(new CustomEvent("CopyPanelClick", { 
        detail: { 
          accept: accept, 
          stop: this.state.checked } }));}, 




    handleAccept: function handleAccept() {
      this._dispatch(true);}, 


    handleCancel: function handleCancel() {
      this._dispatch(false);}, 


    handleToggle: function handleToggle() {
      this.setState({ checked: !this.state.checked });}, 


    render: function render() {
      return (
        React.createElement("div", { className: "copy-content" }, 
        React.createElement("div", { className: "copy-body" }, 
        React.createElement("img", { className: "copy-logo", src: "shared/img/helloicon.svg" }), 
        React.createElement("h1", { className: "copy-message" }, 
        mozL10n.get("copy_panel_message"), 
        React.createElement("label", { className: "copy-toggle-label" }, 
        React.createElement("input", { onChange: this.handleToggle, type: "checkbox" }), 
        mozL10n.get("copy_panel_dont_show_again_label")))), 



        React.createElement("button", { className: "copy-button", onClick: this.handleCancel }, 
        mozL10n.get("copy_panel_cancel_button_label")), 

        React.createElement("button", { className: "copy-button", onClick: this.handleAccept }, 
        mozL10n.get("copy_panel_accept_button_label"))));} });






  /**
   * Copy panel initialization.
   */
  function init() {
    // Wait for all LoopAPI message requests to complete before continuing init.
    var _Cu$import = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});var LoopAPI = _Cu$import.LoopAPI;
    var requests = ["GetAllStrings", "GetLocale", "GetPluralRule"];
    return Promise.all(requests.map(function (name) {return new Promise(function (resolve) {
        LoopAPI.sendMessageToHandler({ name: name }, resolve);});})).
    then(function (results) {
      // Extract individual requested values to initialize l10n.
      var _results = _slicedToArray(results, 3);var stringBundle = _results[0];var locale = _results[1];var pluralRule = _results[2];
      mozL10n.initialize({ 
        getStrings: function getStrings(key) {
          if (!(key in stringBundle)) {
            console.error("No string found for key:", key);}

          return JSON.stringify({ textContent: stringBundle[key] || "" });}, 

        locale: locale, 
        pluralRule: pluralRule });


      ReactDOM.render(React.createElement(CopyView, null), document.querySelector("#main"));

      document.documentElement.setAttribute("dir", mozL10n.language.direction);
      document.documentElement.setAttribute("lang", mozL10n.language.code);});}



  return { 
    CopyView: CopyView, 
    init: init };}(

document.mozL10n);

document.addEventListener("DOMContentLoaded", loop.copy.init);
