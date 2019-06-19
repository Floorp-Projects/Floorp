/** @license React v16.8.6
 * react-test-renderer-shallow.production.min.js
 *
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
(function (global, factory) {
	typeof exports === 'object' && typeof module !== 'undefined' ? module.exports = factory(require('devtools/client/shared/vendor/react')) :
	typeof define === 'function' && define.amd ? define(['devtools/client/shared/vendor/react'], factory) :
	(global.ReactShallowRenderer = factory(global.React));
}(this, (function (React) { 'use strict';

/**
 * Use invariant() to assert state which your program assumes to be true.
 *
 * Provide sprintf-style format (only %s is supported) and arguments
 * to provide information about what broke and what you were
 * expecting.
 *
 * The invariant message will be stripped in production, but the invariant
 * will remain to ensure logic does not differ in production.
 */

function invariant(condition, format, a, b, c, d, e, f) {
  if (!condition) {
    var error = void 0;
    if (format === undefined) {
      error = new Error('Minified exception occurred; use the non-minified dev environment ' + 'for the full error message and additional helpful warnings.');
    } else {
      var args = [a, b, c, d, e, f];
      var argIndex = 0;
      error = new Error(format.replace(/%s/g, function () {
        return args[argIndex++];
      }));
      error.name = 'Invariant Violation';
    }

    error.framesToPop = 1; // we don't care about invariant's own frame
    throw error;
  }
}

// Relying on the `invariant()` implementation lets us
// preserve the format and params in the www builds.
/**
 * WARNING: DO NOT manually require this module.
 * This is a replacement for `invariant(...)` used by the error code system
 * and will _only_ be required by the corresponding babel pass.
 * It always throws.
 */
function reactProdInvariant(code) {
  var argCount = arguments.length - 1;
  var url = 'https://reactjs.org/docs/error-decoder.html?invariant=' + code;
  for (var argIdx = 0; argIdx < argCount; argIdx++) {
    url += '&args[]=' + encodeURIComponent(arguments[argIdx + 1]);
  }
  // Rename it so that our build transform doesn't attempt
  // to replace this invariant() call with reactProdInvariant().
  var i = invariant;
  i(false,
  // The error code is intentionally part of the message (and
  // not the format argument) so that we could deduplicate
  // different errors in logs based on the code.
  'Minified React error #' + code + '; visit %s ' + 'for the full message or use the non-minified dev environment ' + 'for full errors and additional helpful warnings. ', url);
}

var ReactInternals = React.__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED;

var _assign = ReactInternals.assign;

// The Symbol used to tag the ReactElement-like types. If there is no native Symbol
// nor polyfill, then a plain number is used for performance.
var hasSymbol = typeof Symbol === 'function' && Symbol.for;

var REACT_ELEMENT_TYPE = hasSymbol ? Symbol.for('react.element') : 0xeac7;
var REACT_PORTAL_TYPE = hasSymbol ? Symbol.for('react.portal') : 0xeaca;
var REACT_FRAGMENT_TYPE = hasSymbol ? Symbol.for('react.fragment') : 0xeacb;
var REACT_STRICT_MODE_TYPE = hasSymbol ? Symbol.for('react.strict_mode') : 0xeacc;
var REACT_PROFILER_TYPE = hasSymbol ? Symbol.for('react.profiler') : 0xead2;
var REACT_PROVIDER_TYPE = hasSymbol ? Symbol.for('react.provider') : 0xeacd;
var REACT_CONTEXT_TYPE = hasSymbol ? Symbol.for('react.context') : 0xeace;
var REACT_ASYNC_MODE_TYPE = hasSymbol ? Symbol.for('react.async_mode') : 0xeacf;
var REACT_CONCURRENT_MODE_TYPE = hasSymbol ? Symbol.for('react.concurrent_mode') : 0xeacf;
var REACT_FORWARD_REF_TYPE = hasSymbol ? Symbol.for('react.forward_ref') : 0xead0;
var REACT_SUSPENSE_TYPE = hasSymbol ? Symbol.for('react.suspense') : 0xead1;
var REACT_MEMO_TYPE = hasSymbol ? Symbol.for('react.memo') : 0xead3;
var REACT_LAZY_TYPE = hasSymbol ? Symbol.for('react.lazy') : 0xead4;

/**
 * Forked from fbjs/warning:
 * https://github.com/facebook/fbjs/blob/e66ba20ad5be433eb54423f2b097d829324d9de6/packages/fbjs/src/__forks__/warning.js
 *
 * Only change is we use console.warn instead of console.error,
 * and do nothing when 'console' is not supported.
 * This really simplifies the code.
 * ---
 * Similar to invariant but only logs a warning if the condition is not met.
 * This can be used to log issues in development environments in critical
 * paths. Removing the logging code for production environments will keep the
 * same logic and follow the same code paths.
 */

function typeOf(object) {
  if (typeof object === 'object' && object !== null) {
    var $$typeof = object.$$typeof;
    switch ($$typeof) {
      case REACT_ELEMENT_TYPE:
        var type = object.type;

        switch (type) {
          case REACT_ASYNC_MODE_TYPE:
          case REACT_CONCURRENT_MODE_TYPE:
          case REACT_FRAGMENT_TYPE:
          case REACT_PROFILER_TYPE:
          case REACT_STRICT_MODE_TYPE:
          case REACT_SUSPENSE_TYPE:
            return type;
          default:
            var $$typeofType = type && type.$$typeof;

            switch ($$typeofType) {
              case REACT_CONTEXT_TYPE:
              case REACT_FORWARD_REF_TYPE:
              case REACT_PROVIDER_TYPE:
                return $$typeofType;
              default:
                return $$typeof;
            }
        }
      case REACT_LAZY_TYPE:
      case REACT_MEMO_TYPE:
      case REACT_PORTAL_TYPE:
        return $$typeof;
    }
  }

  return undefined;
}

// AsyncMode is deprecated along with isAsyncMode





var ForwardRef = REACT_FORWARD_REF_TYPE;








// AsyncMode should be deprecated





function isForwardRef(object) {
  return typeOf(object) === REACT_FORWARD_REF_TYPE;
}


function isMemo(object) {
  return typeOf(object) === REACT_MEMO_TYPE;
}

var BEFORE_SLASH_RE = /^(.*)[\\\/]/;

var describeComponentFrame = function (name, source, ownerName) {
  var sourceInfo = '';
  if (source) {
    var path = source.fileName;
    var fileName = path.replace(BEFORE_SLASH_RE, '');
    sourceInfo = ' (at ' + fileName + ':' + source.lineNumber + ')';
  } else if (ownerName) {
    sourceInfo = ' (created by ' + ownerName + ')';
  }
  return '\n    in ' + (name || 'Unknown') + sourceInfo;
};

/**
 * Similar to invariant but only logs a warning if the condition is not met.
 * This can be used to log issues in development environments in critical
 * paths. Removing the logging code for production environments will keep the
 * same logic and follow the same code paths.
 */

var Resolved = 1;


function refineResolvedLazyComponent(lazyComponent) {
  return lazyComponent._status === Resolved ? lazyComponent._result : null;
}

function getWrappedName(outerType, innerType, wrapperName) {
  var functionName = innerType.displayName || innerType.name || '';
  return outerType.displayName || (functionName !== '' ? wrapperName + '(' + functionName + ')' : wrapperName);
}

function getComponentName(type) {
  if (type == null) {
    // Host root, text node or just invalid type.
    return null;
  }
  if (typeof type === 'function') {
    return type.displayName || type.name || null;
  }
  if (typeof type === 'string') {
    return type;
  }
  switch (type) {
    case REACT_CONCURRENT_MODE_TYPE:
      return 'ConcurrentMode';
    case REACT_FRAGMENT_TYPE:
      return 'Fragment';
    case REACT_PORTAL_TYPE:
      return 'Portal';
    case REACT_PROFILER_TYPE:
      return 'Profiler';
    case REACT_STRICT_MODE_TYPE:
      return 'StrictMode';
    case REACT_SUSPENSE_TYPE:
      return 'Suspense';
  }
  if (typeof type === 'object') {
    switch (type.$$typeof) {
      case REACT_CONTEXT_TYPE:
        return 'Context.Consumer';
      case REACT_PROVIDER_TYPE:
        return 'Context.Provider';
      case REACT_FORWARD_REF_TYPE:
        return getWrappedName(type, type.render, 'ForwardRef');
      case REACT_MEMO_TYPE:
        return getComponentName(type.type);
      case REACT_LAZY_TYPE:
        {
          var thenable = type;
          var resolvedThenable = refineResolvedLazyComponent(thenable);
          if (resolvedThenable) {
            return getComponentName(resolvedThenable);
          }
        }
    }
  }
  return null;
}

/**
 * inlined Object.is polyfill to avoid requiring consumers ship their own
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/is
 */
function is(x, y) {
  return x === y && (x !== 0 || 1 / x === 1 / y) || x !== x && y !== y // eslint-disable-line no-self-compare
  ;
}

var hasOwnProperty = Object.prototype.hasOwnProperty;

/**
 * Performs equality by iterating through keys on an object and returning false
 * when any key has values which are not strictly equal between the arguments.
 * Returns true when the values of all keys are strictly equal.
 */
function shallowEqual(objA, objB) {
  if (is(objA, objB)) {
    return true;
  }

  if (typeof objA !== 'object' || objA === null || typeof objB !== 'object' || objB === null) {
    return false;
  }

  var keysA = Object.keys(objA);
  var keysB = Object.keys(objB);

  if (keysA.length !== keysB.length) {
    return false;
  }

  // Test for A's keys different from B.
  for (var i = 0; i < keysA.length; i++) {
    if (!hasOwnProperty.call(objB, keysA[i]) || !is(objA[keysA[i]], objB[keysA[i]])) {
      return false;
    }
  }

  return true;
}

/**
 * Copyright (c) 2013-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */



/**
 * Assert that the values match with the type specs.
 * Error messages are memorized and will only be shown once.
 *
 * @param {object} typeSpecs Map of name to a ReactPropType
 * @param {object} values Runtime values that need to be type-checked
 * @param {string} location e.g. "prop", "context", "child context"
 * @param {string} componentName Name of the component for error messages.
 * @param {?Function} getStack Returns the component stack.
 * @private
 */
function checkPropTypes(typeSpecs, values, location, componentName, getStack) {
  
}

var checkPropTypes_1 = checkPropTypes;

var ReactSharedInternals = React.__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED;

// Prevent newer renderers from RTE when used with older react package versions.
// Current owner and dispatcher used to share the same ref,
// but PR #14548 split them out to better support the react-debug-tools package.
if (!ReactSharedInternals.hasOwnProperty('ReactCurrentDispatcher')) {
  ReactSharedInternals.ReactCurrentDispatcher = {
    current: null
  };
}

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

var ReactCurrentDispatcher = ReactSharedInternals.ReactCurrentDispatcher;


var RE_RENDER_LIMIT = 25;

var emptyObject = {};
function areHookInputsEqual(nextDeps, prevDeps) {
  if (prevDeps === null) {
    return false;
  }

  // Don't bother comparing lengths in prod because these arrays should be
  // passed inline.
  if (nextDeps.length !== prevDeps.length) {
    
  }
  for (var i = 0; i < prevDeps.length && i < nextDeps.length; i++) {
    if (is(nextDeps[i], prevDeps[i])) {
      continue;
    }
    return false;
  }
  return true;
}

var Updater = function () {
  function Updater(renderer) {
    _classCallCheck(this, Updater);

    this._renderer = renderer;
    this._callbacks = [];
  }

  Updater.prototype._enqueueCallback = function _enqueueCallback(callback, publicInstance) {
    if (typeof callback === 'function' && publicInstance) {
      this._callbacks.push({
        callback: callback,
        publicInstance: publicInstance
      });
    }
  };

  Updater.prototype._invokeCallbacks = function _invokeCallbacks() {
    var callbacks = this._callbacks;
    this._callbacks = [];

    callbacks.forEach(function (_ref) {
      var callback = _ref.callback,
          publicInstance = _ref.publicInstance;

      callback.call(publicInstance);
    });
  };

  Updater.prototype.isMounted = function isMounted(publicInstance) {
    return !!this._renderer._element;
  };

  Updater.prototype.enqueueForceUpdate = function enqueueForceUpdate(publicInstance, callback, callerName) {
    this._enqueueCallback(callback, publicInstance);
    this._renderer._forcedUpdate = true;
    this._renderer.render(this._renderer._element, this._renderer._context);
  };

  Updater.prototype.enqueueReplaceState = function enqueueReplaceState(publicInstance, completeState, callback, callerName) {
    this._enqueueCallback(callback, publicInstance);
    this._renderer._newState = completeState;
    this._renderer.render(this._renderer._element, this._renderer._context);
  };

  Updater.prototype.enqueueSetState = function enqueueSetState(publicInstance, partialState, callback, callerName) {
    this._enqueueCallback(callback, publicInstance);
    var currentState = this._renderer._newState || publicInstance.state;

    if (typeof partialState === 'function') {
      partialState = partialState.call(publicInstance, currentState, publicInstance.props);
    }

    // Null and undefined are treated as no-ops.
    if (partialState === null || partialState === undefined) {
      return;
    }

    this._renderer._newState = _assign({}, currentState, partialState);

    this._renderer.render(this._renderer._element, this._renderer._context);
  };

  return Updater;
}();

function createHook() {
  return {
    memoizedState: null,
    queue: null,
    next: null
  };
}

function basicStateReducer(state, action) {
  return typeof action === 'function' ? action(state) : action;
}

var ReactShallowRenderer = function () {
  function ReactShallowRenderer() {
    _classCallCheck(this, ReactShallowRenderer);

    this._reset();
  }

  ReactShallowRenderer.prototype._reset = function _reset() {
    this._context = null;
    this._element = null;
    this._instance = null;
    this._newState = null;
    this._rendered = null;
    this._rendering = false;
    this._forcedUpdate = false;
    this._updater = new Updater(this);
    this._dispatcher = this._createDispatcher();
    this._workInProgressHook = null;
    this._firstWorkInProgressHook = null;
    this._isReRender = false;
    this._didScheduleRenderPhaseUpdate = false;
    this._renderPhaseUpdates = null;
    this._numberOfReRenders = 0;
  };

  ReactShallowRenderer.prototype._validateCurrentlyRenderingComponent = function _validateCurrentlyRenderingComponent() {
    !(this._rendering && !this._instance) ? reactProdInvariant('321') : void 0;
  };

  ReactShallowRenderer.prototype._createDispatcher = function _createDispatcher() {
    var _this = this;

    var useReducer = function (reducer, initialArg, init) {
      _this._validateCurrentlyRenderingComponent();
      _this._createWorkInProgressHook();
      var workInProgressHook = _this._workInProgressHook;

      if (_this._isReRender) {
        // This is a re-render.
        var _queue = workInProgressHook.queue;
        var _dispatch = _queue.dispatch;
        if (_this._numberOfReRenders > 0) {
          // Apply the new render phase updates to the previous current hook.
          if (_this._renderPhaseUpdates !== null) {
            // Render phase updates are stored in a map of queue -> linked list
            var firstRenderPhaseUpdate = _this._renderPhaseUpdates.get(_queue);
            if (firstRenderPhaseUpdate !== undefined) {
              _this._renderPhaseUpdates.delete(_queue);
              var _newState = workInProgressHook.memoizedState;
              var _update = firstRenderPhaseUpdate;
              do {
                var _action = _update.action;
                _newState = reducer(_newState, _action);
                _update = _update.next;
              } while (_update !== null);
              workInProgressHook.memoizedState = _newState;
              return [_newState, _dispatch];
            }
          }
          return [workInProgressHook.memoizedState, _dispatch];
        }
        // Process updates outside of render
        var newState = workInProgressHook.memoizedState;
        var update = _queue.first;
        if (update !== null) {
          do {
            var _action2 = update.action;
            newState = reducer(newState, _action2);
            update = update.next;
          } while (update !== null);
          _queue.first = null;
          workInProgressHook.memoizedState = newState;
        }
        return [newState, _dispatch];
      } else {
        var initialState = void 0;
        if (reducer === basicStateReducer) {
          // Special case for `useState`.
          initialState = typeof initialArg === 'function' ? initialArg() : initialArg;
        } else {
          initialState = init !== undefined ? init(initialArg) : initialArg;
        }
        workInProgressHook.memoizedState = initialState;
        var _queue2 = workInProgressHook.queue = {
          first: null,
          dispatch: null
        };
        var _dispatch2 = _queue2.dispatch = _this._dispatchAction.bind(_this, _queue2);
        return [workInProgressHook.memoizedState, _dispatch2];
      }
    };

    var useState = function (initialState) {
      return useReducer(basicStateReducer,
      // useReducer has a special case to support lazy useState initializers
      initialState);
    };

    var useMemo = function (nextCreate, deps) {
      _this._validateCurrentlyRenderingComponent();
      _this._createWorkInProgressHook();

      var nextDeps = deps !== undefined ? deps : null;

      if (_this._workInProgressHook !== null && _this._workInProgressHook.memoizedState !== null) {
        var prevState = _this._workInProgressHook.memoizedState;
        var prevDeps = prevState[1];
        if (nextDeps !== null) {
          if (areHookInputsEqual(nextDeps, prevDeps)) {
            return prevState[0];
          }
        }
      }

      var nextValue = nextCreate();
      _this._workInProgressHook.memoizedState = [nextValue, nextDeps];
      return nextValue;
    };

    var useRef = function (initialValue) {
      _this._validateCurrentlyRenderingComponent();
      _this._createWorkInProgressHook();
      var previousRef = _this._workInProgressHook.memoizedState;
      if (previousRef === null) {
        var ref = { current: initialValue };
        _this._workInProgressHook.memoizedState = ref;
        return ref;
      } else {
        return previousRef;
      }
    };

    var readContext = function (context, observedBits) {
      return context._currentValue;
    };

    var noOp = function () {
      _this._validateCurrentlyRenderingComponent();
    };

    var identity = function (fn) {
      return fn;
    };

    return {
      readContext: readContext,
      useCallback: identity,
      useContext: function (context) {
        _this._validateCurrentlyRenderingComponent();
        return readContext(context);
      },
      useDebugValue: noOp,
      useEffect: noOp,
      useImperativeHandle: noOp,
      useLayoutEffect: noOp,
      useMemo: useMemo,
      useReducer: useReducer,
      useRef: useRef,
      useState: useState
    };
  };

  ReactShallowRenderer.prototype._dispatchAction = function _dispatchAction(queue, action) {
    !(this._numberOfReRenders < RE_RENDER_LIMIT) ? reactProdInvariant('301') : void 0;

    if (this._rendering) {
      // This is a render phase update. Stash it in a lazily-created map of
      // queue -> linked list of updates. After this render pass, we'll restart
      // and apply the stashed updates on top of the work-in-progress hook.
      this._didScheduleRenderPhaseUpdate = true;
      var update = {
        action: action,
        next: null
      };
      var renderPhaseUpdates = this._renderPhaseUpdates;
      if (renderPhaseUpdates === null) {
        this._renderPhaseUpdates = renderPhaseUpdates = new Map();
      }
      var firstRenderPhaseUpdate = renderPhaseUpdates.get(queue);
      if (firstRenderPhaseUpdate === undefined) {
        renderPhaseUpdates.set(queue, update);
      } else {
        // Append the update to the end of the list.
        var lastRenderPhaseUpdate = firstRenderPhaseUpdate;
        while (lastRenderPhaseUpdate.next !== null) {
          lastRenderPhaseUpdate = lastRenderPhaseUpdate.next;
        }
        lastRenderPhaseUpdate.next = update;
      }
    } else {
      var _update2 = {
        action: action,
        next: null
      };

      // Append the update to the end of the list.
      var last = queue.first;
      if (last === null) {
        queue.first = _update2;
      } else {
        while (last.next !== null) {
          last = last.next;
        }
        last.next = _update2;
      }

      // Re-render now.
      this.render(this._element, this._context);
    }
  };

  ReactShallowRenderer.prototype._createWorkInProgressHook = function _createWorkInProgressHook() {
    if (this._workInProgressHook === null) {
      // This is the first hook in the list
      if (this._firstWorkInProgressHook === null) {
        this._isReRender = false;
        this._firstWorkInProgressHook = this._workInProgressHook = createHook();
      } else {
        // There's already a work-in-progress. Reuse it.
        this._isReRender = true;
        this._workInProgressHook = this._firstWorkInProgressHook;
      }
    } else {
      if (this._workInProgressHook.next === null) {
        this._isReRender = false;
        // Append to the end of the list
        this._workInProgressHook = this._workInProgressHook.next = createHook();
      } else {
        // There's already a work-in-progress. Reuse it.
        this._isReRender = true;
        this._workInProgressHook = this._workInProgressHook.next;
      }
    }
    return this._workInProgressHook;
  };

  ReactShallowRenderer.prototype._finishHooks = function _finishHooks(element, context) {
    if (this._didScheduleRenderPhaseUpdate) {
      // Updates were scheduled during the render phase. They are stored in
      // the `renderPhaseUpdates` map. Call the component again, reusing the
      // work-in-progress hooks and applying the additional updates on top. Keep
      // restarting until no more updates are scheduled.
      this._didScheduleRenderPhaseUpdate = false;
      this._numberOfReRenders += 1;

      // Start over from the beginning of the list
      this._workInProgressHook = null;
      this._rendering = false;
      this.render(element, context);
    } else {
      this._workInProgressHook = null;
      this._renderPhaseUpdates = null;
      this._numberOfReRenders = 0;
    }
  };

  ReactShallowRenderer.prototype.getMountedInstance = function getMountedInstance() {
    return this._instance;
  };

  ReactShallowRenderer.prototype.getRenderOutput = function getRenderOutput() {
    return this._rendered;
  };

  ReactShallowRenderer.prototype.render = function render(element) {
    var context = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : emptyObject;

    !React.isValidElement(element) ? reactProdInvariant('12', typeof element === 'function' ? ' Instead of passing a component class, make sure to instantiate ' + 'it by passing it to React.createElement.' : '') : void 0;
    element = element;
    // Show a special message for host elements since it's a common case.
    !(typeof element.type !== 'string') ? reactProdInvariant('13', element.type) : void 0;
    !(isForwardRef(element) || typeof element.type === 'function' || isMemo(element.type)) ? reactProdInvariant('249', Array.isArray(element.type) ? 'array' : element.type === null ? 'null' : typeof element.type) : void 0;

    if (this._rendering) {
      return;
    }
    if (this._element != null && this._element.type !== element.type) {
      this._reset();
    }

    var elementType = isMemo(element.type) ? element.type.type : element.type;
    var previousElement = this._element;

    this._rendering = true;
    this._element = element;
    this._context = getMaskedContext(elementType.contextTypes, context);

    // Inner memo component props aren't currently validated in createElement.
    if (isMemo(element.type) && elementType.propTypes) {
      currentlyValidatingElement = element;
      checkPropTypes_1(elementType.propTypes, element.props, 'prop', getComponentName(elementType), getStackAddendum);
    }

    if (this._instance) {
      this._updateClassComponent(elementType, element, this._context);
    } else {
      if (shouldConstruct(elementType)) {
        this._instance = new elementType(element.props, this._context, this._updater);
        if (typeof elementType.getDerivedStateFromProps === 'function') {
          var partialState = elementType.getDerivedStateFromProps.call(null, element.props, this._instance.state);
          if (partialState != null) {
            this._instance.state = _assign({}, this._instance.state, partialState);
          }
        }

        if (elementType.contextTypes) {
          currentlyValidatingElement = element;
          checkPropTypes_1(elementType.contextTypes, this._context, 'context', getName(elementType, this._instance), getStackAddendum);

          currentlyValidatingElement = null;
        }

        this._mountClassComponent(elementType, element, this._context);
      } else {
        var shouldRender = true;
        if (isMemo(element.type) && previousElement !== null) {
          // This is a Memo component that is being re-rendered.
          var compare = element.type.compare || shallowEqual;
          if (compare(previousElement.props, element.props)) {
            shouldRender = false;
          }
        }
        if (shouldRender) {
          var prevDispatcher = ReactCurrentDispatcher.current;
          ReactCurrentDispatcher.current = this._dispatcher;
          try {
            // elementType could still be a ForwardRef if it was
            // nested inside Memo.
            if (elementType.$$typeof === ForwardRef) {
              !(typeof elementType.render === 'function') ? reactProdInvariant('322', typeof elementType.render) : void 0;
              this._rendered = elementType.render.call(undefined, element.props, element.ref);
            } else {
              this._rendered = elementType(element.props, this._context);
            }
          } finally {
            ReactCurrentDispatcher.current = prevDispatcher;
          }
          this._finishHooks(element, context);
        }
      }
    }

    this._rendering = false;
    this._updater._invokeCallbacks();

    return this.getRenderOutput();
  };

  ReactShallowRenderer.prototype.unmount = function unmount() {
    if (this._instance) {
      if (typeof this._instance.componentWillUnmount === 'function') {
        this._instance.componentWillUnmount();
      }
    }
    this._reset();
  };

  ReactShallowRenderer.prototype._mountClassComponent = function _mountClassComponent(elementType, element, context) {
    this._instance.context = context;
    this._instance.props = element.props;
    this._instance.state = this._instance.state || null;
    this._instance.updater = this._updater;

    if (typeof this._instance.UNSAFE_componentWillMount === 'function' || typeof this._instance.componentWillMount === 'function') {
      var beforeState = this._newState;

      // In order to support react-lifecycles-compat polyfilled components,
      // Unsafe lifecycles should not be invoked for components using the new APIs.
      if (typeof elementType.getDerivedStateFromProps !== 'function' && typeof this._instance.getSnapshotBeforeUpdate !== 'function') {
        if (typeof this._instance.componentWillMount === 'function') {
          this._instance.componentWillMount();
        }
        if (typeof this._instance.UNSAFE_componentWillMount === 'function') {
          this._instance.UNSAFE_componentWillMount();
        }
      }

      // setState may have been called during cWM
      if (beforeState !== this._newState) {
        this._instance.state = this._newState || emptyObject;
      }
    }

    this._rendered = this._instance.render();
    // Intentionally do not call componentDidMount()
    // because DOM refs are not available.
  };

  ReactShallowRenderer.prototype._updateClassComponent = function _updateClassComponent(elementType, element, context) {
    var props = element.props;


    var oldState = this._instance.state || emptyObject;
    var oldProps = this._instance.props;

    if (oldProps !== props) {
      // In order to support react-lifecycles-compat polyfilled components,
      // Unsafe lifecycles should not be invoked for components using the new APIs.
      if (typeof elementType.getDerivedStateFromProps !== 'function' && typeof this._instance.getSnapshotBeforeUpdate !== 'function') {
        if (typeof this._instance.componentWillReceiveProps === 'function') {
          this._instance.componentWillReceiveProps(props, context);
        }
        if (typeof this._instance.UNSAFE_componentWillReceiveProps === 'function') {
          this._instance.UNSAFE_componentWillReceiveProps(props, context);
        }
      }
    }

    // Read state after cWRP in case it calls setState
    var state = this._newState || oldState;
    if (typeof elementType.getDerivedStateFromProps === 'function') {
      var partialState = elementType.getDerivedStateFromProps.call(null, props, state);
      if (partialState != null) {
        state = _assign({}, state, partialState);
      }
    }

    var shouldUpdate = true;
    if (this._forcedUpdate) {
      shouldUpdate = true;
      this._forcedUpdate = false;
    } else if (typeof this._instance.shouldComponentUpdate === 'function') {
      shouldUpdate = !!this._instance.shouldComponentUpdate(props, state, context);
    } else if (elementType.prototype && elementType.prototype.isPureReactComponent) {
      shouldUpdate = !shallowEqual(oldProps, props) || !shallowEqual(oldState, state);
    }

    if (shouldUpdate) {
      // In order to support react-lifecycles-compat polyfilled components,
      // Unsafe lifecycles should not be invoked for components using the new APIs.
      if (typeof elementType.getDerivedStateFromProps !== 'function' && typeof this._instance.getSnapshotBeforeUpdate !== 'function') {
        if (typeof this._instance.componentWillUpdate === 'function') {
          this._instance.componentWillUpdate(props, state, context);
        }
        if (typeof this._instance.UNSAFE_componentWillUpdate === 'function') {
          this._instance.UNSAFE_componentWillUpdate(props, state, context);
        }
      }
    }

    this._instance.context = context;
    this._instance.props = props;
    this._instance.state = state;
    this._newState = null;

    if (shouldUpdate) {
      this._rendered = this._instance.render();
    }
    // Intentionally do not call componentDidUpdate()
    // because DOM refs are not available.
  };

  return ReactShallowRenderer;
}();

ReactShallowRenderer.createRenderer = function () {
  return new ReactShallowRenderer();
};

var currentlyValidatingElement = null;

function getDisplayName(element) {
  if (element == null) {
    return '#empty';
  } else if (typeof element === 'string' || typeof element === 'number') {
    return '#text';
  } else if (typeof element.type === 'string') {
    return element.type;
  } else {
    var elementType = isMemo(element.type) ? element.type.type : element.type;
    return elementType.displayName || elementType.name || 'Unknown';
  }
}

function getStackAddendum() {
  var stack = '';
  if (currentlyValidatingElement) {
    var name = getDisplayName(currentlyValidatingElement);
    var owner = currentlyValidatingElement._owner;
    stack += describeComponentFrame(name, currentlyValidatingElement._source, owner && getComponentName(owner.type));
  }
  return stack;
}

function getName(type, instance) {
  var constructor = instance && instance.constructor;
  return type.displayName || constructor && constructor.displayName || type.name || constructor && constructor.name || null;
}

function shouldConstruct(Component) {
  return !!(Component.prototype && Component.prototype.isReactComponent);
}

function getMaskedContext(contextTypes, unmaskedContext) {
  if (!contextTypes || !unmaskedContext) {
    return emptyObject;
  }
  var context = {};
  for (var key in contextTypes) {
    context[key] = unmaskedContext[key];
  }
  return context;
}



var ReactShallowRenderer$2 = ({
	default: ReactShallowRenderer
});

var ReactShallowRenderer$3 = ( ReactShallowRenderer$2 && ReactShallowRenderer ) || ReactShallowRenderer$2;

// TODO: decide on the top-level export form.
// This is hacky but makes it work with both Rollup and Jest.
var shallow = ReactShallowRenderer$3.default || ReactShallowRenderer$3;

return shallow;

})));
