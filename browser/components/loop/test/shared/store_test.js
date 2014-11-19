/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var expect = chai.expect;

describe("loop.store.createStore", function () {
  "use strict";

  var sandbox;
  var sharedActions = loop.shared.actions;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
  });

  afterEach(function() {
    sandbox.restore();
  });

  it("should create a store constructor", function() {
    expect(loop.store.createStore({})).to.be.a("function");
  });

  it("should implement Backbone.Events", function() {
    expect(loop.store.createStore({}).prototype).to.include.keys(["on", "off"])
  });

  describe("Store API", function() {
    var dispatcher;

    beforeEach(function() {
      dispatcher = new loop.Dispatcher();
    });

    describe("#constructor", function() {
      it("should require a dispatcher", function() {
        var TestStore = loop.store.createStore({});
        expect(function() {
          new TestStore();
        }).to.Throw(/required dispatcher/);
      });

      it("should call initialize() when constructed, if defined", function() {
        var initialize = sandbox.spy();
        var TestStore = loop.store.createStore({initialize: initialize});
        var options = {fake: true};

        new TestStore(dispatcher, options);

        sinon.assert.calledOnce(initialize);
        sinon.assert.calledWithExactly(initialize, options);
      });

      it("should register actions", function() {
        sandbox.stub(dispatcher, "register");
        var TestStore = loop.store.createStore({
          actions: ["a", "b"],
          a: function() {},
          b: function() {}
        });

        var store = new TestStore(dispatcher);

        sinon.assert.calledOnce(dispatcher.register);
        sinon.assert.calledWithExactly(dispatcher.register, store, ["a", "b"]);
      });

      it("should throw if a registered action isn't implemented", function() {
        var TestStore = loop.store.createStore({
          actions: ["a", "b"],
          a: function() {} // missing b
        });

        expect(function() {
          new TestStore(dispatcher);
        }).to.Throw(/should implement an action handler for b/);
      });
    });

    describe("#getInitialStoreState", function() {
      it("should set initial store state if provided", function() {
        var TestStore = loop.store.createStore({
          getInitialStoreState: function() {
            return {foo: "bar"};
          }
        });

        var store = new TestStore(dispatcher);

        expect(store.getStoreState()).eql({foo: "bar"});
      });
    });

    describe("#dispatchAction", function() {
      it("should dispatch an action", function() {
        sandbox.stub(dispatcher, "dispatch");
        var TestStore = loop.store.createStore({});
        var TestAction = sharedActions.Action.define("TestAction", {});
        var action = new TestAction({});
        var store = new TestStore(dispatcher);

        store.dispatchAction(action);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, action);
      });
    });

    describe("#getStoreState", function() {
      var TestStore = loop.store.createStore({});
      var store;

      beforeEach(function() {
        store = new TestStore(dispatcher);
        store.setStoreState({foo: "bar", bar: "baz"});
      });

      it("should retrieve the whole state by default", function() {
        expect(store.getStoreState()).eql({foo: "bar", bar: "baz"});
      });

      it("should retrieve a given property state", function() {
        expect(store.getStoreState("bar")).eql("baz");
      });
    });

    describe("#setStoreState", function() {
      var TestStore = loop.store.createStore({});
      var store;

      beforeEach(function() {
        store = new TestStore(dispatcher);
        store.setStoreState({foo: "bar"});
      });

      it("should update store state data", function() {
        store.setStoreState({foo: "baz"});

        expect(store.getStoreState("foo")).eql("baz");
      });

      it("should trigger a `change` event", function(done) {
        store.once("change", function() {
          done();
        });

        store.setStoreState({foo: "baz"});
      });

      it("should trigger a `change:<prop>` event", function(done) {
        store.once("change:foo", function() {
          done();
        });

        store.setStoreState({foo: "baz"});
      });
    });
  });
});
