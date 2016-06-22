"use strict"; /* Any copyright is dedicated to the Public Domain.
               * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.shared.desktopViews", function () {
  "use strict";

  var expect = chai.expect;
  var l10n = navigator.mozL10n || document.mozL10n;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var sharedDesktopViews = loop.shared.desktopViews;
  var sandbox, clock, dispatcher;

  beforeEach(function () {
    sandbox = LoopMochaUtils.createSandbox();
    clock = sandbox.useFakeTimers(); // exposes sandbox.clock as a fake timer
    sandbox.stub(l10n, "get", function (x) {
      return "translated:" + x;});


    LoopMochaUtils.stubLoopRequest({ 
      GetLoopPref: function GetLoopPref() {
        return true;} });



    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");});


  afterEach(function () {
    sandbox.restore();
    LoopMochaUtils.restore();});


  describe("CopyLinkButton", function () {
    var view;

    function mountTestComponent(props) {
      props = _.extend({ 
        dispatcher: dispatcher, 
        locationForMetrics: "conversation", 
        roomData: { 
          roomUrl: "http://invalid", 
          roomContextUrls: [{ 
            location: "fakeLocation", 
            url: "fakeUrl" }] } }, 


      props || {});
      return TestUtils.renderIntoDocument(
      React.createElement(sharedDesktopViews.CopyLinkButton, props));}


    beforeEach(function () {
      view = mountTestComponent();});


    it("should dispatch a CopyRoomUrl action when the copy button is pressed", function () {
      var copyBtn = ReactDOM.findDOMNode(view);
      React.addons.TestUtils.Simulate.click(copyBtn);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch, new sharedActions.CopyRoomUrl({ 
        roomUrl: "http://invalid", 
        from: "conversation" }));});



    it("should change the text when the url has been copied", function () {
      var copyBtn = ReactDOM.findDOMNode(view);
      React.addons.TestUtils.Simulate.click(copyBtn);

      expect(copyBtn.textContent).eql("translated:invite_copied_link_button");});


    it("should keep the text for a while after the url has been copied", function () {
      var copyBtn = ReactDOM.findDOMNode(view);
      React.addons.TestUtils.Simulate.click(copyBtn);
      clock.tick(sharedDesktopViews.CopyLinkButton.TRIGGERED_RESET_DELAY / 2);

      expect(copyBtn.textContent).eql("translated:invite_copied_link_button");});


    it("should reset the text a bit after the url has been copied", function () {
      var copyBtn = ReactDOM.findDOMNode(view);
      React.addons.TestUtils.Simulate.click(copyBtn);
      clock.tick(sharedDesktopViews.CopyLinkButton.TRIGGERED_RESET_DELAY);

      expect(copyBtn.textContent).eql("translated:invite_copy_link_button");});


    it("should invoke callback if defined", function () {
      var callback = sinon.stub();
      view = mountTestComponent({ 
        callback: callback });

      var copyBtn = ReactDOM.findDOMNode(view);
      React.addons.TestUtils.Simulate.click(copyBtn);
      clock.tick(sharedDesktopViews.CopyLinkButton.TRIGGERED_RESET_DELAY);

      sinon.assert.calledOnce(callback);});});



  describe("EmailLinkButton", function () {
    var view;

    function mountTestComponent(props) {
      props = _.extend({ 
        dispatcher: dispatcher, 
        locationForMetrics: "conversation", 
        roomData: { 
          roomUrl: "http://invalid", 
          roomContextUrls: [] } }, 

      props || {});
      return TestUtils.renderIntoDocument(
      React.createElement(sharedDesktopViews.EmailLinkButton, props));}


    beforeEach(function () {
      view = mountTestComponent();});


    it("should dispatch an EmailRoomUrl with no description" + 
    " for rooms without context when the email button is pressed", 
    function () {
      var emailBtn = ReactDOM.findDOMNode(view);

      React.addons.TestUtils.Simulate.click(emailBtn);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch, 
      new sharedActions.EmailRoomUrl({ 
        roomUrl: "http://invalid", 
        roomDescription: undefined, 
        from: "conversation" }));});



    it("should dispatch an EmailRoomUrl with a domain name description for rooms with context", 
    function () {
      view = mountTestComponent({ 
        roomData: { 
          roomUrl: "http://invalid", 
          roomContextUrls: [{ location: "http://www.mozilla.com/" }] } });



      var emailBtn = ReactDOM.findDOMNode(view);

      React.addons.TestUtils.Simulate.click(emailBtn);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch, 
      new sharedActions.EmailRoomUrl({ 
        roomUrl: "http://invalid", 
        roomDescription: "www.mozilla.com", 
        from: "conversation" }));});



    it("should invoke callback if defined", function () {
      var callback = sinon.stub();
      view = mountTestComponent({ 
        callback: callback });

      var emailBtn = ReactDOM.findDOMNode(view);
      React.addons.TestUtils.Simulate.click(emailBtn);

      sinon.assert.calledOnce(callback);});});



  describe("FacebookShareButton", function () {
    var view;

    function mountTestComponent(props) {
      props = _.extend({ 
        dispatcher: dispatcher, 
        locationForMetrics: "conversation", 
        roomData: { 
          roomUrl: "http://invalid", 
          roomContextUrls: [] } }, 

      props || {});
      return TestUtils.renderIntoDocument(
      React.createElement(sharedDesktopViews.FacebookShareButton, props));}


    it("should dispatch a FacebookShareRoomUrl action when the facebook button is clicked", 
    function () {
      view = mountTestComponent();

      var facebookBtn = ReactDOM.findDOMNode(view);

      React.addons.TestUtils.Simulate.click(facebookBtn);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch, 
      new sharedActions.FacebookShareRoomUrl({ 
        from: "conversation", 
        roomUrl: "http://invalid" }));});



    it("should invoke callback if defined", function () {
      var callback = sinon.stub();
      view = mountTestComponent({ 
        callback: callback });

      var facebookBtn = ReactDOM.findDOMNode(view);
      React.addons.TestUtils.Simulate.click(facebookBtn);

      sinon.assert.calledOnce(callback);});});



  describe("SharePanelView", function () {
    var view;

    function mountTestComponent(props) {
      props = _.extend({ 
        dispatcher: dispatcher, 
        facebookEnabled: false, 
        locationForMetrics: "conversation", 
        roomData: { roomUrl: "http://invalid" }, 
        savingContext: false, 
        show: true, 
        showEditContext: false }, 
      props);
      return TestUtils.renderIntoDocument(
      React.createElement(sharedDesktopViews.SharePanelView, props));}


    it("should not display the Facebook Share button when it is disabled in prefs", 
    function () {
      view = mountTestComponent({ 
        facebookEnabled: false });


      expect(ReactDOM.findDOMNode(view).querySelectorAll(".btn-facebook")).
      to.have.length.of(0);});


    it("should display the Facebook Share button only when it is enabled in prefs", 
    function () {
      view = mountTestComponent({ 
        facebookEnabled: true });


      expect(ReactDOM.findDOMNode(view).querySelectorAll(".btn-facebook")).
      to.have.length.of(1);});


    it("should not display the panel when show prop is false", function () {
      view = mountTestComponent({ 
        show: false });


      expect(ReactDOM.findDOMNode(view)).eql(null);});


    it("should not display the panel when roomUrl is not defined", function () {
      view = mountTestComponent({ 
        roomData: {} });


      expect(ReactDOM.findDOMNode(view)).eql(null);});});});
