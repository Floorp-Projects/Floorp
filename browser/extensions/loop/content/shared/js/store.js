/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.createStore = (function() {
  "use strict";

  var baseStorePrototype = {
    __registerActions: function(actions) {
      // check that store methods are implemented
      actions.forEach(function(handler) {
        if (typeof this[handler] !== "function") {
          throw new Error("Store should implement an action handler for " +
                          handler);
        }
      }, this);
      this.dispatcher.register(this, actions);
    },

    /**
     * Proxy helper for dispatching an action from this store.
     *
     * @param  {sharedAction.Action} action The action to dispatch.
     */
    dispatchAction: function(action) {
      this.dispatcher.dispatch(action);
    },

    /**
     * Returns current store state. You can request a given state property by
     * providing the `key` argument.
     *
     * @param  {String|undefined} key An optional state property name.
     * @return {Mixed}
     */
    getStoreState: function(key) {
      return key ? this._storeState[key] : this._storeState;
    },

    /**
     * Updates store state and trigger a global "change" event, plus one for
     * each provided newState property.
     *
     * @param {Object} newState The new store state object.
     */
    setStoreState: function(newState) {
      for (var key in newState) {
        this._storeState[key] = newState[key];
        this.trigger("change:" + key);
      }
      this.trigger("change");
    },

    /**
     * Resets the store state to the initially defined state.
     */
    resetStoreState: function() {
      if (typeof this.getInitialStoreState === "function") {
        this._storeState = this.getInitialStoreState();
      } else {
        this._storeState = {};
      }
    }
  };

  /**
   * Creates a new Store constructor.
   *
   * @param  {Object}   storeProto The store prototype.
   * @return {Function}            A store constructor.
   */
  function createStore(storeProto) {
    var BaseStore = function(dispatcher, options) {
      options = options || {};

      if (!dispatcher) {
        throw new Error("Missing required dispatcher");
      }
      this.dispatcher = dispatcher;
      if (Array.isArray(this.actions)) {
        this.__registerActions(this.actions);
      }

      if (typeof this.initialize === "function") {
        this.initialize(options);
      }

      if (typeof this.getInitialStoreState === "function") {
        this._storeState = this.getInitialStoreState();
      } else {
        this._storeState = {};
      }
    };
    BaseStore.prototype = _.extend({}, // destination object
                                   Backbone.Events,
                                   baseStorePrototype,
                                   storeProto);
    return BaseStore;
  }

  return createStore;
})();

/**
 * Store mixin generator. Usage:
 *
 *     StoreMixin.register({roomStore: new RoomStore(…)});
 *     var Comp = React.createClass({
 *       mixins: [StoreMixin("roomStore")]
 *     });
 */
loop.store.StoreMixin = (function() {
  "use strict";

  var _stores = {};
  function StoreMixin(id) {
    return {
      getStore: function() {
        // Allows the ui-showcase to override the specified store.
        if (id in this.props) {
          return this.props[id];
        }
        if (!_stores[id]) {
          throw new Error("Unavailable store " + id);
        }
        return _stores[id];
      },
      getStoreState: function() {
        return this.getStore().getStoreState();
      },
      componentWillMount: function() {
        this.getStore().on("change", function() {
          this.setState(this.getStoreState());
        }, this);
      },
      componentWillUnmount: function() {
        this.getStore().off("change", null, this);
      }
    };
  }
  StoreMixin.register = function(stores) {
    _.extend(_stores, stores);
  };
  /**
   * Used for test purposes, to clear the list of registered stores.
   */
  StoreMixin.clearRegisteredStores = function() {
    _stores = {};
  };
  return StoreMixin;
})();
