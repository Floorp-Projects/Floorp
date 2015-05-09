/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Object.freeze({
  // TODO: Bug 727854 Use same implementation than common JS modules,
  // i.e. EventEmitter module

  /**
   * Create an EventEmitter instance.
   */
  createEventEmitter: function createEventEmitter(emit) {
    let listeners = Object.create(null);
    let eventEmitter = Object.freeze({
      emit: emit,
      on: function on(name, callback) {
        if (typeof callback !== "function")
          return this;
        if (!(name in listeners))
          listeners[name] = [];
        listeners[name].push(callback);
        return this;
      },
      once: function once(name, callback) {
        eventEmitter.on(name, function onceCallback() {
          eventEmitter.removeListener(name, onceCallback);
          callback.apply(callback, arguments);
        });
      },
      removeListener: function removeListener(name, callback) {
        if (!(name in listeners))
          return;
        let index = listeners[name].indexOf(callback);
        if (index == -1)
          return;
        listeners[name].splice(index, 1);
      }
    });
    function onEvent(name) {
      if (!(name in listeners))
        return [];
      let args = Array.slice(arguments, 1);
      let results = [];
      for (let callback of listeners[name]) {
        results.push(callback.apply(null, args));
      }
      return results;
    }
    return {
      eventEmitter: eventEmitter,
      emit: onEvent
    };
  },

  /**
   * Create an EventEmitter instance to communicate with chrome module
   * by passing only strings between compartments.
   * This function expects `emitToChrome` function, that allows to send
   * events to the chrome module. It returns the EventEmitter as `pipe`
   * attribute, and, `onChromeEvent` a function that allows chrome module
   * to send event into the EventEmitter.
   *
   *                  pipe.emit --> emitToChrome
   *              onChromeEvent --> callback registered through pipe.on
   */
  createPipe: function createPipe(emitToChrome) {
    let ContentWorker = this;
    function onEvent(type, ...args) {
      // JSON.stringify is buggy with cross-sandbox values,
      // it may return "{}" on functions. Use a replacer to match them correctly.
      let replacer = (k, v) =>
        typeof(v) === "function"
          ? (type === "console" ? Function.toString.call(v) : void(0))
          : v;

      let str = JSON.stringify([type, ...args], replacer);
      emitToChrome(str);
    }

    let { eventEmitter, emit } =
      ContentWorker.createEventEmitter(onEvent);

    return {
      pipe: eventEmitter,
      onChromeEvent: function onChromeEvent(array) {
        // We either receive a stringified array, or a real array.
        // We still allow to pass an array of objects, in WorkerSandbox.emitSync
        // in order to allow sending DOM node reference between content script
        // and modules (only used for context-menu API)
        let args = typeof array == "string" ? JSON.parse(array) : array;
        return emit.apply(null, args);
      }
    };
  },

  injectConsole: function injectConsole(exports, pipe) {
    exports.console = Object.freeze({
      log: pipe.emit.bind(null, "console", "log"),
      info: pipe.emit.bind(null, "console", "info"),
      warn: pipe.emit.bind(null, "console", "warn"),
      error: pipe.emit.bind(null, "console", "error"),
      debug: pipe.emit.bind(null, "console", "debug"),
      exception: pipe.emit.bind(null, "console", "exception"),
      trace: pipe.emit.bind(null, "console", "trace"),
      time: pipe.emit.bind(null, "console", "time"),
      timeEnd: pipe.emit.bind(null, "console", "timeEnd")
    });
  },

  injectTimers: function injectTimers(exports, chromeAPI, pipe, console) {
    // wrapped functions from `'timer'` module.
    // Wrapper adds `try catch` blocks to the callbacks in order to
    // emit `error` event if exception is thrown in
    // the Worker global scope.
    // @see http://www.w3.org/TR/workers/#workerutils

    // List of all living timeouts/intervals
    let _timers = Object.create(null);

    // Keep a reference to original timeout functions
    let {
      setTimeout: chromeSetTimeout,
      setInterval: chromeSetInterval,
      clearTimeout: chromeClearTimeout,
      clearInterval: chromeClearInterval
    } = chromeAPI.timers;

    function registerTimer(timer) {
      let registerMethod = null;
      if (timer.kind == "timeout")
        registerMethod = chromeSetTimeout;
      else if (timer.kind == "interval")
        registerMethod = chromeSetInterval;
      else
        throw new Error("Unknown timer kind: " + timer.kind);

      if (typeof timer.fun == 'string') {
        let code = timer.fun;
        timer.fun = () => chromeAPI.sandbox.evaluate(exports, code);
      } else if (typeof timer.fun != 'function') {
        throw new Error('Unsupported callback type' + typeof timer.fun);
      }

      let id = registerMethod(onFire, timer.delay);
      function onFire() {
        try {
          if (timer.kind == "timeout")
            delete _timers[id];
          timer.fun.apply(null, timer.args);
        } catch(e) {
          console.exception(e);
          let wrapper = {
            instanceOfError: instanceOf(e, Error),
            value: e,
          };
          if (wrapper.instanceOfError) {
            wrapper.value = {
              message: e.message,
              fileName: e.fileName,
              lineNumber: e.lineNumber,
              stack: e.stack,
              name: e.name,
            };
          }
          pipe.emit('error', wrapper);
        }
      }
      _timers[id] = timer;
      return id;
    }

    // copied from sdk/lang/type.js since modules are not available here
    function instanceOf(value, Type) {
      var isConstructorNameSame;
      var isConstructorSourceSame;

      // If `instanceof` returned `true` we know result right away.
      var isInstanceOf = value instanceof Type;

      // If `instanceof` returned `false` we do ducktype check since `Type` may be
      // from a different sandbox. If a constructor of the `value` or a constructor
      // of the value's prototype has same name and source we assume that it's an
      // instance of the Type.
      if (!isInstanceOf && value) {
        isConstructorNameSame = value.constructor.name === Type.name;
        isConstructorSourceSame = String(value.constructor) == String(Type);
        isInstanceOf = (isConstructorNameSame && isConstructorSourceSame) ||
                        instanceOf(Object.getPrototypeOf(value), Type);
      }
      return isInstanceOf;
    }

    function unregisterTimer(id) {
      if (!(id in _timers))
        return;
      let { kind } = _timers[id];
      delete _timers[id];
      if (kind == "timeout")
        chromeClearTimeout(id);
      else if (kind == "interval")
        chromeClearInterval(id);
      else
        throw new Error("Unknown timer kind: " + kind);
    }

    function disableAllTimers() {
      Object.keys(_timers).forEach(unregisterTimer);
    }

    exports.setTimeout = function ContentScriptSetTimeout(callback, delay) {
      return registerTimer({
        kind: "timeout",
        fun: callback,
        delay: delay,
        args: Array.slice(arguments, 2)
      });
    };
    exports.clearTimeout = function ContentScriptClearTimeout(id) {
      unregisterTimer(id);
    };

    exports.setInterval = function ContentScriptSetInterval(callback, delay) {
      return registerTimer({
        kind: "interval",
        fun: callback,
        delay: delay,
        args: Array.slice(arguments, 2)
      });
    };
    exports.clearInterval = function ContentScriptClearInterval(id) {
      unregisterTimer(id);
    };

    // On page-hide, save a list of all existing timers before disabling them,
    // in order to be able to restore them on page-show.
    // These events are fired when the page goes in/out of bfcache.
    // https://developer.mozilla.org/En/Working_with_BFCache
    let frozenTimers = [];
    pipe.on("pageshow", function onPageShow() {
      frozenTimers.forEach(registerTimer);
    });
    pipe.on("pagehide", function onPageHide() {
      frozenTimers = [];
      for (let id in _timers)
        frozenTimers.push(_timers[id]);
      disableAllTimers();
      // Some other pagehide listeners may register some timers that won't be
      // frozen as this particular pagehide listener is called first.
      // So freeze these timers on next cycle.
      chromeSetTimeout(function () {
        for (let id in _timers)
          frozenTimers.push(_timers[id]);
        disableAllTimers();
      }, 0);
    });

    // Unregister all timers when the page is destroyed
    // (i.e. when it is removed from bfcache)
    pipe.on("detach", function clearTimeouts() {
      disableAllTimers();
      _timers = {};
      frozenTimers = [];
    });
  },

  injectMessageAPI: function injectMessageAPI(exports, pipe, console) {

    let ContentWorker = this;
    let { eventEmitter: port, emit : portEmit } =
      ContentWorker.createEventEmitter(pipe.emit.bind(null, "event"));
    pipe.on("event", portEmit);

    let self = {
      port: port,
      postMessage: pipe.emit.bind(null, "message"),
      on: pipe.on.bind(null),
      once: pipe.once.bind(null),
      removeListener: pipe.removeListener.bind(null),
    };
    Object.defineProperty(exports, "self", {
      value: self
    });
  },

  injectOptions: function (exports, options) {
    Object.defineProperty( exports.self, "options", { value: JSON.parse( options ) });
  },

  inject: function (exports, chromeAPI, emitToChrome, options) {
    let ContentWorker = this;
    let { pipe, onChromeEvent } =
      ContentWorker.createPipe(emitToChrome);

    ContentWorker.injectConsole(exports, pipe);
    ContentWorker.injectTimers(exports, chromeAPI, pipe, exports.console);
    ContentWorker.injectMessageAPI(exports, pipe, exports.console);
    if ( options !== undefined ) {
      ContentWorker.injectOptions(exports, options);
    }

    Object.freeze( exports.self );

    return onChromeEvent;
  }
});
