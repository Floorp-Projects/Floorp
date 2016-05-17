"use strict"; /* Any copyright is dedicated to the Public Domain.
               * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loopapi-client", function () {
  "use strict";

  var expect = chai.expect;
  var sandbox;

  beforeEach(function () {
    sandbox = sinon.sandbox.create();
    window.addMessageListener = sinon.stub();
    window.removeMessageListener = sinon.stub();
    window.sendAsyncMessage = sinon.stub();});


  afterEach(function () {
    loop.request.reset();
    loop.subscribe.reset();
    sandbox.restore();

    delete window.addMessageListener;
    delete window.removeMessageListener;
    delete window.sendAsyncMessage;});


  describe("loop.request", function () {
    it("should send a message", function () {
      var promise = loop.request("GetLoopPref", "enabled");

      expect(promise).to.be.an.instanceof(Promise);
      sinon.assert.calledOnce(window.sendAsyncMessage);
      sinon.assert.calledWithExactly(window.sendAsyncMessage, "Loop:Message", 
      [loop._lastMessageID, "GetLoopPref", "enabled"]);
      sinon.assert.calledOnce(window.addMessageListener);

      // Call the added listener, so that the promise resolves.
      window.addMessageListener.args[0][1]({ 
        name: "Loop:Message", 
        data: [loop._lastMessageID, "true"] });


      return promise;});


    it("should correct the command name", function () {
      var promise = loop.request("getLoopPref", "enabled");

      sinon.assert.calledWithExactly(window.sendAsyncMessage, "Loop:Message", 
      [loop._lastMessageID, "GetLoopPref", "enabled"]);

      // Call the added listener, so that the promise resolves.
      window.addMessageListener.args[0][1]({ 
        name: "Loop:Message", 
        data: [loop._lastMessageID, "true"] });


      return promise;});


    it("should pass all arguments in-order", function () {
      var promise = loop.request("SetLoopPref", "enabled", false, 1, 2, 3);

      sinon.assert.calledWithExactly(window.sendAsyncMessage, "Loop:Message", 
      [loop._lastMessageID, "SetLoopPref", "enabled", false, 1, 2, 3]);

      // Call the added listener, so that the promise resolves.
      window.addMessageListener.args[0][1]({ 
        name: "Loop:Message", 
        data: [loop._lastMessageID, "true"] });


      return promise;});


    it("should resolve the promise when a response is received", function () {
      var listener;
      window.addMessageListener = function (name, callback) {
        listener = callback;};


      var promise = loop.request("GetLoopPref", "enabled").then(function (result) {
        expect(result).to.eql("result");});


      listener({ 
        data: [loop._lastMessageID, "result"] });


      return promise;});


    it("should not start listening for messages more than once", function () {
      return new Promise(function (resolve) {
        loop.request("GetLoopPref", "enabled").then(function () {
          sinon.assert.calledOnce(window.addMessageListener);

          loop.request("GetLoopPref", "enabled").then(function () {
            sinon.assert.calledOnce(window.addMessageListener);

            resolve();});


          // Call the added listener, so that the promise resolves.
          window.addMessageListener.args[0][1]({ 
            name: "Loop:Message", 
            data: [loop._lastMessageID, "true"] });});



        // Call the added listener, so that the promise resolves.
        window.addMessageListener.args[0][1]({ 
          name: "Loop:Message", 
          data: [loop._lastMessageID, "true"] });});});});





  describe("loop.storeRequest", function () {
    afterEach(function () {
      loop.storedRequests = {};});


    it("should the result of a request", function () {
      loop.storeRequest(["GetLoopPref"], true);

      expect(loop.storedRequests).to.deep.equal({ 
        "GetLoopPref": true });});



    it("should the result of a request with multiple params", function () {
      loop.storeRequest(["GetLoopPref", "enabled", "or", "not", "well", 
      "perhaps", true, 2], true);

      expect(loop.storedRequests).to.deep.equal({ 
        "GetLoopPref|enabled|or|not|well|perhaps|true|2": true });});});




  describe("loop.getStoredRequest", function () {
    afterEach(function () {
      loop.storedRequests = {};});


    it("should retrieve a result", function () {
      loop.storedRequests["GetLoopPref"] = true;

      expect(loop.getStoredRequest(["GetLoopPref"])).to.eql(true);});


    it("should return log and return null for invalid requests", function () {
      sandbox.stub(console, "error");

      expect(loop.getStoredRequest(["SomethingNeverStored"])).to.eql(null);
      sinon.assert.calledOnce(console.error);
      sinon.assert.calledWithExactly(console.error, 
      "This request has not been stored!", ["SomethingNeverStored"]);});});



  describe("loop.requestMulti", function () {
    it("should send a batch of messages", function () {
      var promise = loop.requestMulti(
      ["GetLoopPref", "enabled"], 
      ["GetLoopPref", "e10s.enabled"]);


      expect(promise).to.be.an.instanceof(Promise);
      sinon.assert.calledOnce(window.sendAsyncMessage);
      sinon.assert.calledWithExactly(window.sendAsyncMessage, "Loop:Message", 
      [loop._lastMessageID, "Batch", [
      [loop._lastMessageID - 2, "GetLoopPref", "enabled"], 
      [loop._lastMessageID - 1, "GetLoopPref", "e10s.enabled"]]]);


      // Call the added listener, so that the promise resolves.
      window.addMessageListener.args[0][1]({ 
        name: "Loop:Message", 
        data: [loop._lastMessageID, "true"] });


      return promise;});


    it("should correct command names", function () {
      var promise = loop.requestMulti(
      ["GetLoopPref", "enabled"], 
      // Use lowercase 'g' on purpose, it should get corrected:
      ["getLoopPref", "e10s.enabled"]);


      sinon.assert.calledWithExactly(window.sendAsyncMessage, "Loop:Message", 
      [loop._lastMessageID, "Batch", [
      [loop._lastMessageID - 2, "GetLoopPref", "enabled"], 
      [loop._lastMessageID - 1, "GetLoopPref", "e10s.enabled"]]]);


      // Call the added listener, so that the promise resolves.
      window.addMessageListener.args[0][1]({ 
        name: "Loop:Message", 
        data: [loop._lastMessageID, "true"] });


      return promise;});


    it("should resolve the promise when a response is received", function () {
      var listener;
      window.addMessageListener = function (name, callback) {
        listener = callback;};


      var promise = loop.requestMulti(
      ["GetLoopPref", "enabled"], 
      ["GetLoopPref", "e10s.enabled"]).
      then(function (result) {
        expect(result).to.eql(["result1", "result2"]);});


      listener({ 
        data: [
        loop._lastMessageID, { 
          "1": "result1", 
          "2": "result2" }] });




      return promise;});


    it("should throw an error when no requests are passed in", function () {
      expect(loop.requestMulti).to.throw(Error, /please pass in a list of calls/);});


    it("should throw when invalid request is passed in", function () {
      expect(loop.requestMulti.bind(null, ["command"], null)).to.
      throw(Error, /each call must be an array of options/);

      expect(loop.requestMulti.bind(null, null, ["command"])).to.
      throw(Error, /each call must be an array of options/);});});



  describe("loop.subscribe", function () {
    var sendMessage = null;
    var callCount = 0;

    beforeEach(function () {
      callCount = 0;
      window.addMessageListener = function (name, callback) {
        sendMessage = callback;
        ++callCount;};});



    afterEach(function () {
      sendMessage = null;});


    it("subscribe to a push message", function () {
      loop.subscribe("LoopStatusChanged", function () {});
      var subscriptions = loop.subscribe.inspect();

      expect(callCount).to.eql(1);
      expect(Object.getOwnPropertyNames(subscriptions).length).to.eql(1);
      expect(subscriptions.LoopStatusChanged.length).to.eql(1);});


    it("should start listening for push messages when a subscriber registers", function () {
      loop.subscribe("LoopStatusChanged", function () {});
      expect(callCount).to.eql(1);

      loop.subscribe("LoopStatusChanged", function () {});
      expect(callCount).to.eql(1);

      loop.subscribe("Test", function () {});
      expect(callCount).to.eql(1);});


    it("incoming push messages should invoke subscriptions", function () {
      var stub1 = sinon.stub();
      var stub2 = sinon.stub();
      var stub3 = sinon.stub();

      loop.subscribe("LoopStatusChanged", stub1);
      loop.subscribe("LoopStatusChanged", stub2);
      loop.subscribe("LoopStatusChanged", stub3);

      sendMessage({ data: ["Foo", ["bar"]] });

      sinon.assert.notCalled(stub1);
      sinon.assert.notCalled(stub2);
      sinon.assert.notCalled(stub3);

      sendMessage({ data: ["LoopStatusChanged", ["Foo", "Bar"]] });

      sinon.assert.calledOnce(stub1);
      sinon.assert.calledWithExactly(stub1, "Foo", "Bar");
      sinon.assert.calledOnce(stub2);
      sinon.assert.calledWithExactly(stub2, "Foo", "Bar");
      sinon.assert.calledOnce(stub3);
      sinon.assert.calledWithExactly(stub3, "Foo", "Bar");});


    it("should invoke subscription with non-array arguments too", function () {
      var stub = sinon.stub();
      loop.subscribe("LoopStatusChanged", stub);

      sendMessage({ data: ["LoopStatusChanged", "Foo"] });

      sinon.assert.calledOnce(stub);
      sinon.assert.calledWithExactly(stub, "Foo");});});



  describe("unsubscribe", function () {
    it("should remove subscriptions from the map", function () {
      var handler = function handler() {};
      loop.subscribe("LoopStatusChanged", handler);

      loop.unsubscribe("LoopStatusChanged", handler);
      expect(loop.subscribe.inspect().LoopStatusChanged.length).to.eql(0);});


    it("should not remove a subscription when a different handler is passed in", function () {
      var handler = function handler() {};
      loop.subscribe("LoopStatusChanged", handler);

      loop.unsubscribe("LoopStatusChanged", function () {});
      expect(loop.subscribe.inspect().LoopStatusChanged.length).to.eql(1);});


    it("should not throw when unsubscribing from an unknown subscription", function () {
      loop.unsubscribe("foobar");});});



  describe("unsubscribeAll", function () {
    it("should clear all present subscriptions", function () {
      loop.subscribe("LoopStatusChanged", function () {});

      expect(Object.getOwnPropertyNames(loop.subscribe.inspect()).length).to.eql(1);

      loop.unsubscribeAll();

      expect(Object.getOwnPropertyNames(loop.subscribe.inspect()).length).to.eql(0);});});});
