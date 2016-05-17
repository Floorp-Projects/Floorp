"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.copy", function () {
  "use strict";

  var expect = chai.expect;
  var CopyView = loop.copy.CopyView;
  var l10n = navigator.mozL10n || document.mozL10n;
  var TestUtils = React.addons.TestUtils;
  var sandbox;

  beforeEach(function () {
    sandbox = LoopMochaUtils.createSandbox();
    sandbox.stub(l10n, "get");});


  afterEach(function () {
    loop.shared.mixins.setRootObject(window);
    sandbox.restore();
    LoopMochaUtils.restore();});


  describe("#init", function () {
    beforeEach(function () {
      sandbox.stub(ReactDOM, "render");
      sandbox.stub(document.mozL10n, "initialize");});


    it("should initalize L10n", function () {
      loop.copy.init();

      sinon.assert.calledOnce(document.mozL10n.initialize);});


    it("should render the copy view", function () {
      loop.copy.init();

      sinon.assert.calledOnce(ReactDOM.render);
      sinon.assert.calledWith(ReactDOM.render, sinon.match(function (value) {
        return value.type === CopyView;}));});});




  describe("#render", function () {
    var view;

    beforeEach(function () {
      view = TestUtils.renderIntoDocument(React.createElement(CopyView));});


    it("should have an unchecked box", function () {
      expect(ReactDOM.findDOMNode(view).querySelector("input[type=checkbox]").checked).eql(false);});


    it("should have two buttons", function () {
      expect(ReactDOM.findDOMNode(view).querySelectorAll("button").length).eql(2);});});



  describe("handleChanges", function () {
    var view;
    beforeEach(function () {
      view = TestUtils.renderIntoDocument(React.createElement(CopyView));});


    it("should have default state !checked", function () {
      expect(view.state.checked).to.be.false;});


    it("should have checked state after change", function () {
      TestUtils.Simulate.change(ReactDOM.findDOMNode(view).querySelector("input"), { 
        target: { checked: true } });


      expect(view.state.checked).to.be.true;});});



  describe("handleClicks", function () {
    var view;

    beforeEach(function () {
      sandbox.stub(window, "dispatchEvent");
      view = TestUtils.renderIntoDocument(React.createElement(CopyView));});


    function checkDispatched(detail) {
      sinon.assert.calledOnce(window.dispatchEvent);
      sinon.assert.calledWith(window.dispatchEvent, sinon.match.has("detail", detail));}


    it("should dispatch accept !stop on accept", function () {
      TestUtils.Simulate.click(ReactDOM.findDOMNode(view).querySelector("button:last-child"));

      checkDispatched({ accept: true, stop: false });});


    it("should dispatch !accept !stop on cancel", function () {
      TestUtils.Simulate.click(ReactDOM.findDOMNode(view).querySelector("button"));

      checkDispatched({ accept: false, stop: false });});


    it("should dispatch accept stop on checked accept", function () {
      TestUtils.Simulate.change(ReactDOM.findDOMNode(view).querySelector("input"), { 
        target: { checked: true } });

      TestUtils.Simulate.click(ReactDOM.findDOMNode(view).querySelector("button:last-child"));

      checkDispatched({ accept: true, stop: true });});


    it("should dispatch !accept stop on checked cancel", function () {
      TestUtils.Simulate.change(ReactDOM.findDOMNode(view).querySelector("input"), { 
        target: { checked: true } });

      TestUtils.Simulate.click(ReactDOM.findDOMNode(view).querySelector("button"));

      checkDispatched({ accept: false, stop: true });});});});
