const React = require("react");
const {mount, shallow} = require("enzyme");
const {IntlProvider, intlShape} = require("react-intl");
const messages = require("data/locales.json")["en-US"];
const intlProvider = new IntlProvider({locale: "en-US", messages});
const {intl} = intlProvider.getChildContext();

/**
 * GlobalOverrider - Utility that allows you to override properties on the global object.
 *                   See unit-entry.js for example usage.
 */
class GlobalOverrider {
  constructor() {
    this.originalGlobals = new Map();
    this.sandbox = sinon.sandbox.create();
  }

  /**
   * _override - Internal method to override properties on the global object.
   *             The first time a given key is overridden, we cache the original
   *             value in this.originalGlobals so that later it can be restored.
   *
   * @param  {string} key The identifier of the property
   * @param  {any} value The value to which the property should be reassigned
   */
  _override(key, value) {
    if (key === "Components") {
      // Components can be reassigned, but it will subsequently throw a deprecation
      // error in Firefox which will stop execution. Adding the assignment statement
      // to a try/catch block will prevent this from happening.
      try {
        global[key] = value;
      } catch (e) {} // eslint-disable-line no-empty
      return;
    }
    if (!this.originalGlobals.has(key)) {
      this.originalGlobals.set(key, global[key]);
    }
    global[key] = value;
  }

  /**
   * set - Override a given property, or all properties on an object
   *
   * @param  {string|object} key If a string, the identifier of the property
   *                             If an object, a number of properties and values to which they should be reassigned.
   * @param  {any} value The value to which the property should be reassigned
   * @return {type}       description
   */
  set(key, value) {
    if (!value && typeof key === "object") {
      const overrides = key;
      Object.keys(overrides).forEach(k => this._override(k, overrides[k]));
    } else {
      this._override(key, value);
    }
    return value;
  }

  /**
   * reset - Reset the global sandbox, so all state on spies, stubs etc. is cleared.
   *         You probably want to call this after each test.
   */
  reset() {
    this.sandbox.reset();
  }

  /**
   * restore - Restore the global sandbox and reset all overriden properties to
   *           their original values. You should call this after all tests have completed.
   */
  restore() {
    this.sandbox.restore();
    this.originalGlobals.forEach((value, key) => {
      global[key] = value;
    });
  }
}

/**
 * Very simple fake for the most basic semantics of nsIPrefBranch. Lots of
 * things aren't yet supported.  Feel free to add them in.
 *
 * @param {Object} args - optional arguments
 * @param {Function} args.initHook - if present, will be called back
 *                   inside the constructor. Typically used from tests
 *                   to save off a pointer to the created instance so that
 *                   stubs and spies can be inspected by the test code.
 */
class FakensIPrefBranch {
  constructor(args) {
    if (args) {
      if ("initHook" in args) {
        args.initHook.call(this);
      }
    }
    this._prefBranch = {};
    this.observers = {};
  }
  addObserver(prefName, callback) {
    this.observers[prefName] = callback;
  }
  removeObserver(prefName, callback) {
    if (prefName in this.observers) {
      delete this.observers[prefName];
    }
  }
  observeBranch(listener) {}
  ignoreBranch(listener) {}
  setStringPref(prefName) {}

  getStringPref(prefName) { return this.get(prefName); }
  getBoolPref(prefName) { return this.get(prefName); }
  get(prefName) { return this.prefs[prefName]; }
  setBoolPref(prefName, value) {
    this.prefs[prefName] = value;

    if (prefName in this.observers) {
      this.observers[prefName]("", "", prefName);
    }
  }
}
FakensIPrefBranch.prototype.prefs = {};

/**
 * Very simple fake for the most basic semantics of Preferences.jsm.
 * Extends FakensIPrefBranch.
 */
class FakePrefs extends FakensIPrefBranch {
  observe(prefName, callback) {
    super.addObserver(prefName, callback);
  }
  ignore(prefName, callback) {
    super.removeObserver(prefName, callback);
  }
  set(prefName, value) {
    this.prefs[prefName] = value;

    if (prefName in this.observers) {
      this.observers[prefName](value);
    }
  }
}

/**
 * Slimmed down version of toolkit/modules/EventEmitter.jsm
 */
function EventEmitter() {}
EventEmitter.decorate = function(objectToDecorate) {
  let emitter = new EventEmitter();
  objectToDecorate.on = emitter.on.bind(emitter);
  objectToDecorate.off = emitter.off.bind(emitter);
  objectToDecorate.once = emitter.once.bind(emitter);
  objectToDecorate.emit = emitter.emit.bind(emitter);
};
EventEmitter.prototype = {
  on(event, listener) {
    if (!this._eventEmitterListeners) {
      this._eventEmitterListeners = new Map();
    }
    if (!this._eventEmitterListeners.has(event)) {
      this._eventEmitterListeners.set(event, []);
    }
    this._eventEmitterListeners.get(event).push(listener);
  },
  off(event, listener) {
    if (!this._eventEmitterListeners) {
      return;
    }
    let listeners = this._eventEmitterListeners.get(event);
    if (listeners) {
      this._eventEmitterListeners.set(event, listeners.filter(
        l => l !== listener && l._originalListener !== listener
      ));
    }
  },
  once(event, listener) {
    return new Promise(resolve => {
      let handler = (_, first, ...rest) => {
        this.off(event, handler);
        if (listener) {
          listener(event, first, ...rest);
        }
        resolve(first);
      };

      handler._originalListener = listener;
      this.on(event, handler);
    });
  },
  // All arguments to this method will be sent to listeners
  emit(event, ...args) {
    if (!this._eventEmitterListeners || !this._eventEmitterListeners.has(event)) {
      return;
    }
    let originalListeners = this._eventEmitterListeners.get(event);
    for (let listener of this._eventEmitterListeners.get(event)) {
      // If the object was destroyed during event emission, stop
      // emitting.
      if (!this._eventEmitterListeners) {
        break;
      }
      // If listeners were removed during emission, make sure the
      // event handler we're going to fire wasn't removed.
      if (originalListeners === this._eventEmitterListeners.get(event) ||
        this._eventEmitterListeners.get(event).some(l => l === listener)) {
        try {
          listener(event, ...args);
        } catch (ex) {
          // error with a listener
        }
      }
    }
  }
};

function FakePerformance() {}
FakePerformance.prototype = {
  marks: new Map(),
  now() {
    return window.performance.now();
  },
  timing: {navigationStart: 222222.123},
  get timeOrigin() {
    return 10000.234;
  },
  // XXX assumes type == "mark"
  getEntriesByName(name, type) {
    if (this.marks.has(name)) {
      return this.marks.get(name);
    }
    return [];
  },
  callsToMark: 0,

  /**
   * @note The "startTime" for each mark is simply the number of times mark
   * has been called in this object.
   */
  mark(name) {
    let markObj = {
      name,
      "entryType": "mark",
      "startTime": ++this.callsToMark,
      "duration": 0
    };

    if (this.marks.has(name)) {
      this.marks.get(name).push(markObj);
      return;
    }

    this.marks.set(name, [markObj]);
  }
};

/**
 * addNumberReducer - a simple dummy reducer for testing that adds a number
 */
function addNumberReducer(prevState = 0, action) {
  return action.type === "ADD" ? prevState + action.data : prevState;
}

/**
 * Helper functions to test components that need IntlProvider as an ancestor
 */
function nodeWithIntlProp(node) {
  return React.cloneElement(node, {intl});
}

function shallowWithIntl(node, options = {}) {
  return shallow(nodeWithIntlProp(node), Object.assign({}, options, {context: {intl}}));
}

function mountWithIntl(node, options = {}) {
  return mount(nodeWithIntlProp(node), Object.assign({}, options, {
    context: {intl},
    childContextTypes: {intl: intlShape}
  }));
}

module.exports = {
  FakePerformance,
  FakePrefs,
  EventEmitter,
  GlobalOverrider,
  addNumberReducer,
  mountWithIntl,
  shallowWithIntl
};
