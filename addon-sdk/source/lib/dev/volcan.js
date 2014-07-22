!function(e){if("object"==typeof exports)module.exports=e();else if("function"==typeof define&&define.amd)define(e);else{var f;"undefined"!=typeof window?f=window:"undefined"!=typeof global?f=global:"undefined"!=typeof self&&(f=self),f.volcan=e()}}(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);throw new Error("Cannot find module '"+o+"'")}var f=n[o]={exports:{}};t[o][0].call(f.exports,function(e){var n=t[o][1][e];return s(n?n:e)},f,f.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(_dereq_,module,exports){
"use strict";

var Client = _dereq_("../client").Client;

function connect(port) {
  var client = new Client();
  return client.connect(port);
}
exports.connect = connect;

},{"../client":4}],2:[function(_dereq_,module,exports){
"use strict";

exports.Promise = Promise;

},{}],3:[function(_dereq_,module,exports){
"use strict";

var describe = Object.getOwnPropertyDescriptor;
var Class = function(fields) {
  var names = Object.keys(fields);
  var constructor = names.indexOf("constructor") >= 0 ? fields.constructor :
                    function() {};
  var ancestor = fields.extends || Object;

  var descriptor = names.reduce(function(descriptor, key) {
    descriptor[key] = describe(fields, key);
    return descriptor;
  }, {});

  var prototype = Object.create(ancestor.prototype, descriptor);

  constructor.prototype = prototype;
  prototype.constructor = constructor;

  return constructor;
};
exports.Class = Class;

},{}],4:[function(_dereq_,module,exports){
"use strict";

var Class = _dereq_("./class").Class;
var TypeSystem = _dereq_("./type-system").TypeSystem;
var values = _dereq_("./util").values;
var Promise = _dereq_("es6-promise").Promise;

var specification = _dereq_("./specification/core.json");

function recoverActorDescriptions(error) {
  console.warn("Failed to fetch protocol specification (see reason below). " +
               "Using a fallback protocal specification!",
               error);
  return _dereq_("./specification/protocol.json");
}

// Type to represent superviser actor relations to actors they supervise
// in terms of lifetime management.
var Supervisor = Class({
  constructor: function(id) {
    this.id = id;
    this.workers = [];
  }
});

var Telemetry = Class({
  add: function(id, ms) {
    console.log("telemetry::", id, ms)
  }
});

// Consider making client a root actor.

var Client = Class({
  constructor: function() {
    this.root = null;
    this.telemetry = new Telemetry();

    this.setupConnection();
    this.setupLifeManagement();
    this.setupTypeSystem();
  },

  setupConnection: function() {
    this.requests = [];
  },
  setupLifeManagement: function() {
    this.cache = Object.create(null);
    this.graph = Object.create(null);
    this.get = this.get.bind(this);
    this.release = this.release.bind(this);
  },
  setupTypeSystem: function() {
    this.typeSystem = new TypeSystem(this);
    this.typeSystem.registerTypes(specification);
  },

  connect: function(port) {
    var client = this;
    return new Promise(function(resolve, reject) {
      client.port = port;
      port.onmessage = client.receive.bind(client);
      client.onReady = resolve;
      client.onFail = reject;

      port.start();
    });
  },
  send: function(packet) {
    this.port.postMessage(packet);
  },
  request: function(packet) {
    var client = this;
    return new Promise(function(resolve, reject) {
      client.requests.push(packet.to, { resolve: resolve, reject: reject });
      client.send(packet);
    });
  },

  receive: function(event) {
    var packet = event.data;
    if (!this.root) {
      if (packet.from !== "root")
        throw Error("Initial packet must be from root");
      if (!("applicationType" in packet))
        throw Error("Initial packet must contain applicationType field");

      this.root = this.typeSystem.read("root", null, "root");
      this.root
          .protocolDescription()
          .catch(recoverActorDescriptions)
          .then(this.typeSystem.registerTypes.bind(this.typeSystem))
          .then(this.onReady.bind(this, this.root), this.onFail);
    } else {
      var actor = this.get(packet.from) || this.root;
      var event = actor.events[packet.type];
      if (event) {
        actor.dispatchEvent(event.read(packet));
      } else {
        var index = this.requests.indexOf(actor.id);
        if (index >= 0) {
          var request = this.requests.splice(index, 2).pop();
          if (packet.error)
            request.reject(packet);
          else
            request.resolve(packet);
        } else {
          console.error(Error("Unexpected packet " + JSON.stringify(packet, 2, 2)),
                        packet,
                        this.requests.slice(0));
        }
      }
    }
  },

  get: function(id) {
    return this.cache[id];
  },
  supervisorOf: function(actor) {
    for (var id in this.graph) {
      if (this.graph[id].indexOf(actor.id) >= 0) {
        return id;
      }
    }
  },
  workersOf: function(actor) {
    return this.graph[actor.id];
  },
  supervise: function(actor, worker) {
    var workers = this.workersOf(actor)
    if (workers.indexOf(worker.id) < 0) {
      workers.push(worker.id);
    }
  },
  unsupervise: function(actor, worker) {
    var workers = this.workersOf(actor);
    var index = workers.indexOf(worker.id)
    if (index >= 0) {
      workers.splice(index, 1)
    }
  },

  register: function(actor) {
    var registered = this.get(actor.id);
    if (!registered) {
      this.cache[actor.id] = actor;
      this.graph[actor.id] = [];
    } else if (registered !== actor) {
      throw new Error("Different actor with same id is already registered");
    }
  },
  unregister: function(actor) {
    if (this.get(actor.id)) {
      delete this.cache[actor.id];
      delete this.graph[actor.id];
    }
  },

  release: function(actor) {
    var supervisor = this.supervisorOf(actor);
    if (supervisor)
      this.unsupervise(supervisor, actor);

    var workers = this.workersOf(actor)

    if (workers) {
      workers.map(this.get).forEach(this.release)
    }
    this.unergister(actor);
  }
});
exports.Client = Client;

},{"./class":3,"./specification/core.json":23,"./specification/protocol.json":24,"./type-system":25,"./util":26,"es6-promise":2}],5:[function(_dereq_,module,exports){
"use strict";

var Symbol = _dereq_("es6-symbol")
var EventEmitter = _dereq_("events").EventEmitter;
var Class = _dereq_("./class").Class;

var $bound = Symbol("EventTarget/handleEvent");
var $emitter = Symbol("EventTarget/emitter");

function makeHandler(handler) {
  return function(event) {
    handler.handleEvent(event);
  }
}

var EventTarget = Class({
  constructor: function() {
    Object.defineProperty(this, $emitter, {
      enumerable: false,
      configurable: true,
      writable: true,
      value: new EventEmitter()
    });
  },
  addEventListener: function(type, handler) {
    if (typeof(handler) === "function") {
      this[$emitter].on(type, handler);
    }
    else if (handler && typeof(handler) === "object") {
      if (!handler[$bound]) handler[$bound] = makeHandler(handler);
      this[$emitter].on(type, handler[$bound]);
    }
  },
  removeEventListener: function(type, handler) {
    if (typeof(handler) === "function")
      this[$emitter].removeListener(type, handler);
    else if (handler && handler[$bound])
      this[$emitter].removeListener(type, handler[$bound]);
  },
  dispatchEvent: function(event) {
    event.target = this;
    this[$emitter].emit(event.type, event);
  }
});
exports.EventTarget = EventTarget;

var MessageEvent = Class({
  constructor: function(type, options) {
    options = options || {};
    this.type = type;
    this.data = options.data === void(0) ? null : options.data;

    this.lastEventId = options.lastEventId || "";
    this.origin = options.origin || "";
    this.bubbles = options.bubbles || false;
    this.cancelable = options.cancelable || false;
  },
  source: null,
  ports: null,
  preventDefault: function() {
  },
  stopPropagation: function() {
  },
  stopImmediatePropagation: function() {
  }
});
exports.MessageEvent = MessageEvent;

},{"./class":3,"es6-symbol":7,"events":6}],6:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

function EventEmitter() {
  this._events = this._events || {};
  this._maxListeners = this._maxListeners || undefined;
}
module.exports = EventEmitter;

// Backwards-compat with node 0.10.x
EventEmitter.EventEmitter = EventEmitter;

EventEmitter.prototype._events = undefined;
EventEmitter.prototype._maxListeners = undefined;

// By default EventEmitters will print a warning if more than 10 listeners are
// added to it. This is a useful default which helps finding memory leaks.
EventEmitter.defaultMaxListeners = 10;

// Obviously not all Emitters should be limited to 10. This function allows
// that to be increased. Set to zero for unlimited.
EventEmitter.prototype.setMaxListeners = function(n) {
  if (!isNumber(n) || n < 0 || isNaN(n))
    throw TypeError('n must be a positive number');
  this._maxListeners = n;
  return this;
};

EventEmitter.prototype.emit = function(type) {
  var er, handler, len, args, i, listeners;

  if (!this._events)
    this._events = {};

  // If there is no 'error' event listener then throw.
  if (type === 'error') {
    if (!this._events.error ||
        (isObject(this._events.error) && !this._events.error.length)) {
      er = arguments[1];
      if (er instanceof Error) {
        throw er; // Unhandled 'error' event
      } else {
        throw TypeError('Uncaught, unspecified "error" event.');
      }
      return false;
    }
  }

  handler = this._events[type];

  if (isUndefined(handler))
    return false;

  if (isFunction(handler)) {
    switch (arguments.length) {
      // fast cases
      case 1:
        handler.call(this);
        break;
      case 2:
        handler.call(this, arguments[1]);
        break;
      case 3:
        handler.call(this, arguments[1], arguments[2]);
        break;
      // slower
      default:
        len = arguments.length;
        args = new Array(len - 1);
        for (i = 1; i < len; i++)
          args[i - 1] = arguments[i];
        handler.apply(this, args);
    }
  } else if (isObject(handler)) {
    len = arguments.length;
    args = new Array(len - 1);
    for (i = 1; i < len; i++)
      args[i - 1] = arguments[i];

    listeners = handler.slice();
    len = listeners.length;
    for (i = 0; i < len; i++)
      listeners[i].apply(this, args);
  }

  return true;
};

EventEmitter.prototype.addListener = function(type, listener) {
  var m;

  if (!isFunction(listener))
    throw TypeError('listener must be a function');

  if (!this._events)
    this._events = {};

  // To avoid recursion in the case that type === "newListener"! Before
  // adding it to the listeners, first emit "newListener".
  if (this._events.newListener)
    this.emit('newListener', type,
              isFunction(listener.listener) ?
              listener.listener : listener);

  if (!this._events[type])
    // Optimize the case of one listener. Don't need the extra array object.
    this._events[type] = listener;
  else if (isObject(this._events[type]))
    // If we've already got an array, just append.
    this._events[type].push(listener);
  else
    // Adding the second element, need to change to array.
    this._events[type] = [this._events[type], listener];

  // Check for listener leak
  if (isObject(this._events[type]) && !this._events[type].warned) {
    var m;
    if (!isUndefined(this._maxListeners)) {
      m = this._maxListeners;
    } else {
      m = EventEmitter.defaultMaxListeners;
    }

    if (m && m > 0 && this._events[type].length > m) {
      this._events[type].warned = true;
      console.error('(node) warning: possible EventEmitter memory ' +
                    'leak detected. %d listeners added. ' +
                    'Use emitter.setMaxListeners() to increase limit.',
                    this._events[type].length);
      console.trace();
    }
  }

  return this;
};

EventEmitter.prototype.on = EventEmitter.prototype.addListener;

EventEmitter.prototype.once = function(type, listener) {
  if (!isFunction(listener))
    throw TypeError('listener must be a function');

  var fired = false;

  function g() {
    this.removeListener(type, g);

    if (!fired) {
      fired = true;
      listener.apply(this, arguments);
    }
  }

  g.listener = listener;
  this.on(type, g);

  return this;
};

// emits a 'removeListener' event iff the listener was removed
EventEmitter.prototype.removeListener = function(type, listener) {
  var list, position, length, i;

  if (!isFunction(listener))
    throw TypeError('listener must be a function');

  if (!this._events || !this._events[type])
    return this;

  list = this._events[type];
  length = list.length;
  position = -1;

  if (list === listener ||
      (isFunction(list.listener) && list.listener === listener)) {
    delete this._events[type];
    if (this._events.removeListener)
      this.emit('removeListener', type, listener);

  } else if (isObject(list)) {
    for (i = length; i-- > 0;) {
      if (list[i] === listener ||
          (list[i].listener && list[i].listener === listener)) {
        position = i;
        break;
      }
    }

    if (position < 0)
      return this;

    if (list.length === 1) {
      list.length = 0;
      delete this._events[type];
    } else {
      list.splice(position, 1);
    }

    if (this._events.removeListener)
      this.emit('removeListener', type, listener);
  }

  return this;
};

EventEmitter.prototype.removeAllListeners = function(type) {
  var key, listeners;

  if (!this._events)
    return this;

  // not listening for removeListener, no need to emit
  if (!this._events.removeListener) {
    if (arguments.length === 0)
      this._events = {};
    else if (this._events[type])
      delete this._events[type];
    return this;
  }

  // emit removeListener for all listeners on all events
  if (arguments.length === 0) {
    for (key in this._events) {
      if (key === 'removeListener') continue;
      this.removeAllListeners(key);
    }
    this.removeAllListeners('removeListener');
    this._events = {};
    return this;
  }

  listeners = this._events[type];

  if (isFunction(listeners)) {
    this.removeListener(type, listeners);
  } else {
    // LIFO order
    while (listeners.length)
      this.removeListener(type, listeners[listeners.length - 1]);
  }
  delete this._events[type];

  return this;
};

EventEmitter.prototype.listeners = function(type) {
  var ret;
  if (!this._events || !this._events[type])
    ret = [];
  else if (isFunction(this._events[type]))
    ret = [this._events[type]];
  else
    ret = this._events[type].slice();
  return ret;
};

EventEmitter.listenerCount = function(emitter, type) {
  var ret;
  if (!emitter._events || !emitter._events[type])
    ret = 0;
  else if (isFunction(emitter._events[type]))
    ret = 1;
  else
    ret = emitter._events[type].length;
  return ret;
};

function isFunction(arg) {
  return typeof arg === 'function';
}

function isNumber(arg) {
  return typeof arg === 'number';
}

function isObject(arg) {
  return typeof arg === 'object' && arg !== null;
}

function isUndefined(arg) {
  return arg === void 0;
}

},{}],7:[function(_dereq_,module,exports){
'use strict';

module.exports = _dereq_('./is-implemented')() ? Symbol : _dereq_('./polyfill');

},{"./is-implemented":8,"./polyfill":22}],8:[function(_dereq_,module,exports){
'use strict';

module.exports = function () {
	var symbol;
	if (typeof Symbol !== 'function') return false;
	symbol = Symbol('test symbol');
	try {
		if (String(symbol) !== 'Symbol (test symbol)') return false;
	} catch (e) { return false; }
	if (typeof Symbol.iterator === 'symbol') return true;

	// Return 'true' for polyfills
	if (typeof Symbol.isConcatSpreadable !== 'object') return false;
	if (typeof Symbol.isRegExp !== 'object') return false;
	if (typeof Symbol.iterator !== 'object') return false;
	if (typeof Symbol.toPrimitive !== 'object') return false;
	if (typeof Symbol.toStringTag !== 'object') return false;
	if (typeof Symbol.unscopables !== 'object') return false;

	return true;
};

},{}],9:[function(_dereq_,module,exports){
'use strict';

var assign        = _dereq_('es5-ext/object/assign')
  , normalizeOpts = _dereq_('es5-ext/object/normalize-options')
  , isCallable    = _dereq_('es5-ext/object/is-callable')
  , contains      = _dereq_('es5-ext/string/#/contains')

  , d;

d = module.exports = function (dscr, value/*, options*/) {
	var c, e, w, options, desc;
	if ((arguments.length < 2) || (typeof dscr !== 'string')) {
		options = value;
		value = dscr;
		dscr = null;
	} else {
		options = arguments[2];
	}
	if (dscr == null) {
		c = w = true;
		e = false;
	} else {
		c = contains.call(dscr, 'c');
		e = contains.call(dscr, 'e');
		w = contains.call(dscr, 'w');
	}

	desc = { value: value, configurable: c, enumerable: e, writable: w };
	return !options ? desc : assign(normalizeOpts(options), desc);
};

d.gs = function (dscr, get, set/*, options*/) {
	var c, e, options, desc;
	if (typeof dscr !== 'string') {
		options = set;
		set = get;
		get = dscr;
		dscr = null;
	} else {
		options = arguments[3];
	}
	if (get == null) {
		get = undefined;
	} else if (!isCallable(get)) {
		options = get;
		get = set = undefined;
	} else if (set == null) {
		set = undefined;
	} else if (!isCallable(set)) {
		options = set;
		set = undefined;
	}
	if (dscr == null) {
		c = true;
		e = false;
	} else {
		c = contains.call(dscr, 'c');
		e = contains.call(dscr, 'e');
	}

	desc = { get: get, set: set, configurable: c, enumerable: e };
	return !options ? desc : assign(normalizeOpts(options), desc);
};

},{"es5-ext/object/assign":10,"es5-ext/object/is-callable":13,"es5-ext/object/normalize-options":17,"es5-ext/string/#/contains":19}],10:[function(_dereq_,module,exports){
'use strict';

module.exports = _dereq_('./is-implemented')()
	? Object.assign
	: _dereq_('./shim');

},{"./is-implemented":11,"./shim":12}],11:[function(_dereq_,module,exports){
'use strict';

module.exports = function () {
	var assign = Object.assign, obj;
	if (typeof assign !== 'function') return false;
	obj = { foo: 'raz' };
	assign(obj, { bar: 'dwa' }, { trzy: 'trzy' });
	return (obj.foo + obj.bar + obj.trzy) === 'razdwatrzy';
};

},{}],12:[function(_dereq_,module,exports){
'use strict';

var keys  = _dereq_('../keys')
  , value = _dereq_('../valid-value')

  , max = Math.max;

module.exports = function (dest, src/*, …srcn*/) {
	var error, i, l = max(arguments.length, 2), assign;
	dest = Object(value(dest));
	assign = function (key) {
		try { dest[key] = src[key]; } catch (e) {
			if (!error) error = e;
		}
	};
	for (i = 1; i < l; ++i) {
		src = arguments[i];
		keys(src).forEach(assign);
	}
	if (error !== undefined) throw error;
	return dest;
};

},{"../keys":14,"../valid-value":18}],13:[function(_dereq_,module,exports){
// Deprecated

'use strict';

module.exports = function (obj) { return typeof obj === 'function'; };

},{}],14:[function(_dereq_,module,exports){
'use strict';

module.exports = _dereq_('./is-implemented')()
	? Object.keys
	: _dereq_('./shim');

},{"./is-implemented":15,"./shim":16}],15:[function(_dereq_,module,exports){
'use strict';

module.exports = function () {
	try {
		Object.keys('primitive');
		return true;
	} catch (e) { return false; }
};

},{}],16:[function(_dereq_,module,exports){
'use strict';

var keys = Object.keys;

module.exports = function (object) {
	return keys(object == null ? object : Object(object));
};

},{}],17:[function(_dereq_,module,exports){
'use strict';

var assign = _dereq_('./assign')

  , forEach = Array.prototype.forEach
  , create = Object.create, getPrototypeOf = Object.getPrototypeOf

  , process;

process = function (src, obj) {
	var proto = getPrototypeOf(src);
	return assign(proto ? process(proto, obj) : obj, src);
};

module.exports = function (options/*, …options*/) {
	var result = create(null);
	forEach.call(arguments, function (options) {
		if (options == null) return;
		process(Object(options), result);
	});
	return result;
};

},{"./assign":10}],18:[function(_dereq_,module,exports){
'use strict';

module.exports = function (value) {
	if (value == null) throw new TypeError("Cannot use null or undefined");
	return value;
};

},{}],19:[function(_dereq_,module,exports){
'use strict';

module.exports = _dereq_('./is-implemented')()
	? String.prototype.contains
	: _dereq_('./shim');

},{"./is-implemented":20,"./shim":21}],20:[function(_dereq_,module,exports){
'use strict';

var str = 'razdwatrzy';

module.exports = function () {
	if (typeof str.contains !== 'function') return false;
	return ((str.contains('dwa') === true) && (str.contains('foo') === false));
};

},{}],21:[function(_dereq_,module,exports){
'use strict';

var indexOf = String.prototype.indexOf;

module.exports = function (searchString/*, position*/) {
	return indexOf.call(this, searchString, arguments[1]) > -1;
};

},{}],22:[function(_dereq_,module,exports){
'use strict';

var d = _dereq_('d')

  , create = Object.create, defineProperties = Object.defineProperties
  , generateName, Symbol;

generateName = (function () {
	var created = create(null);
	return function (desc) {
		var postfix = 0;
		while (created[desc + (postfix || '')]) ++postfix;
		desc += (postfix || '');
		created[desc] = true;
		return '@@' + desc;
	};
}());

module.exports = Symbol = function (description) {
	var symbol;
	if (this instanceof Symbol) {
		throw new TypeError('TypeError: Symbol is not a constructor');
	}
	symbol = create(Symbol.prototype);
	description = (description === undefined ? '' : String(description));
	return defineProperties(symbol, {
		__description__: d('', description),
		__name__: d('', generateName(description))
	});
};

Object.defineProperties(Symbol, {
	create: d('', Symbol('create')),
	hasInstance: d('', Symbol('hasInstance')),
	isConcatSpreadable: d('', Symbol('isConcatSpreadable')),
	isRegExp: d('', Symbol('isRegExp')),
	iterator: d('', Symbol('iterator')),
	toPrimitive: d('', Symbol('toPrimitive')),
	toStringTag: d('', Symbol('toStringTag')),
	unscopables: d('', Symbol('unscopables'))
});

defineProperties(Symbol.prototype, {
	properToString: d(function () {
		return 'Symbol (' + this.__description__ + ')';
	}),
	toString: d('', function () { return this.__name__; })
});
Object.defineProperty(Symbol.prototype, Symbol.toPrimitive, d('',
	function (hint) {
		throw new TypeError("Conversion of symbol objects is not allowed");
	}));
Object.defineProperty(Symbol.prototype, Symbol.toStringTag, d('c', 'Symbol'));

},{"d":9}],23:[function(_dereq_,module,exports){
module.exports={
  "types": {
    "root": {
      "category": "actor",
      "typeName": "root",
      "methods": [
        {
          "name": "echo",
          "request": {
            "string": { "_arg": 0, "type": "string" }
          },
          "response": {
            "string": { "_retval": "string" }
          }
        },
        {
          "name": "listTabs",
          "request": {},
          "response": { "_retval": "tablist" }
        },
        {
          "name": "protocolDescription",
          "request": {},
          "response": { "_retval": "json" }
        }
      ],
      "events": {
        "tabListChanged": {}
      }
    },
    "tablist": {
      "category": "dict",
      "typeName": "tablist",
      "specializations": {
        "selected": "number",
        "tabs": "array:tab",
        "url": "string",
        "consoleActor": "console",
        "inspectorActor": "inspector",
        "styleSheetsActor": "stylesheets",
        "styleEditorActor": "styleeditor",
        "memoryActor": "memory",
        "eventLoopLagActor": "eventLoopLag",
        "preferenceActor": "preference",
        "deviceActor": "device",

        "profilerActor": "profiler",
        "chromeDebugger": "chromeDebugger",
        "webappsActor": "webapps"
      }
    },
    "tab": {
      "category": "actor",
      "typeName": "tab",
      "fields": {
        "title": "string",
        "url": "string",
        "outerWindowID": "number",
        "inspectorActor": "inspector",
        "callWatcherActor": "call-watcher",
        "canvasActor": "canvas",
        "webglActor": "webgl",
        "webaudioActor": "webaudio",
        "storageActor": "storage",
        "gcliActor": "gcli",
        "memoryActor": "memory",
        "eventLoopLag": "eventLoopLag",
        "styleSheetsActor": "stylesheets",
        "styleEditorActor": "styleeditor",

        "consoleActor": "console",
        "traceActor": "trace"
      }
    }
  }
}

},{}],24:[function(_dereq_,module,exports){
module.exports={
  "types": {
    "longstractor": {
      "category": "actor",
      "typeName": "longstractor",
      "methods": [
        {
          "name": "substring",
          "request": {
            "type": "substring",
            "start": {
              "_arg": 0,
              "type": "primitive"
            },
            "end": {
              "_arg": 1,
              "type": "primitive"
            }
          },
          "response": {
            "substring": {
              "_retval": "primitive"
            }
          }
        },
        {
          "name": "release",
          "release": true,
          "request": {
            "type": "release"
          },
          "response": {}
        }
      ],
      "events": {}
    },
    "stylesheet": {
      "category": "actor",
      "typeName": "stylesheet",
      "methods": [
        {
          "name": "toggleDisabled",
          "request": {
            "type": "toggleDisabled"
          },
          "response": {
            "disabled": {
              "_retval": "boolean"
            }
          }
        },
        {
          "name": "getText",
          "request": {
            "type": "getText"
          },
          "response": {
            "text": {
              "_retval": "longstring"
            }
          }
        },
        {
          "name": "getOriginalSources",
          "request": {
            "type": "getOriginalSources"
          },
          "response": {
            "originalSources": {
              "_retval": "nullable:array:originalsource"
            }
          }
        },
        {
          "name": "getOriginalLocation",
          "request": {
            "type": "getOriginalLocation",
            "line": {
              "_arg": 0,
              "type": "number"
            },
            "column": {
              "_arg": 1,
              "type": "number"
            }
          },
          "response": {
            "_retval": "originallocationresponse"
          }
        },
        {
          "name": "update",
          "request": {
            "type": "update",
            "text": {
              "_arg": 0,
              "type": "string"
            },
            "transition": {
              "_arg": 1,
              "type": "boolean"
            }
          },
          "response": {}
        }
      ],
      "events": {
        "property-change": {
          "type": "propertyChange",
          "property": {
            "_arg": 0,
            "type": "string"
          },
          "value": {
            "_arg": 1,
            "type": "json"
          }
        },
        "style-applied": {
          "type": "styleApplied"
        }
      }
    },
    "originalsource": {
      "category": "actor",
      "typeName": "originalsource",
      "methods": [
        {
          "name": "getText",
          "request": {
            "type": "getText"
          },
          "response": {
            "text": {
              "_retval": "longstring"
            }
          }
        }
      ],
      "events": {}
    },
    "stylesheets": {
      "category": "actor",
      "typeName": "stylesheets",
      "methods": [
        {
          "name": "getStyleSheets",
          "request": {
            "type": "getStyleSheets"
          },
          "response": {
            "styleSheets": {
              "_retval": "array:stylesheet"
            }
          }
        },
        {
          "name": "addStyleSheet",
          "request": {
            "type": "addStyleSheet",
            "text": {
              "_arg": 0,
              "type": "string"
            }
          },
          "response": {
            "styleSheet": {
              "_retval": "stylesheet"
            }
          }
        }
      ],
      "events": {}
    },
    "originallocationresponse": {
      "category": "dict",
      "typeName": "originallocationresponse",
      "specializations": {
        "source": "string",
        "line": "number",
        "column": "number"
      }
    },
    "domnode": {
      "category": "actor",
      "typeName": "domnode",
      "methods": [
        {
          "name": "getNodeValue",
          "request": {
            "type": "getNodeValue"
          },
          "response": {
            "value": {
              "_retval": "longstring"
            }
          }
        },
        {
          "name": "setNodeValue",
          "request": {
            "type": "setNodeValue",
            "value": {
              "_arg": 0,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "getImageData",
          "request": {
            "type": "getImageData",
            "maxDim": {
              "_arg": 0,
              "type": "nullable:number"
            }
          },
          "response": {
            "_retval": "imageData"
          }
        },
        {
          "name": "modifyAttributes",
          "request": {
            "type": "modifyAttributes",
            "modifications": {
              "_arg": 0,
              "type": "array:json"
            }
          },
          "response": {}
        }
      ],
      "events": {}
    },
    "appliedstyle": {
      "category": "dict",
      "typeName": "appliedstyle",
      "specializations": {
        "rule": "domstylerule#actorid",
        "inherited": "nullable:domnode#actorid"
      }
    },
    "matchedselector": {
      "category": "dict",
      "typeName": "matchedselector",
      "specializations": {
        "rule": "domstylerule#actorid",
        "selector": "string",
        "value": "string",
        "status": "number"
      }
    },
    "matchedselectorresponse": {
      "category": "dict",
      "typeName": "matchedselectorresponse",
      "specializations": {
        "rules": "array:domstylerule",
        "sheets": "array:stylesheet",
        "matched": "array:matchedselector"
      }
    },
    "appliedStylesReturn": {
      "category": "dict",
      "typeName": "appliedStylesReturn",
      "specializations": {
        "entries": "array:appliedstyle",
        "rules": "array:domstylerule",
        "sheets": "array:stylesheet"
      }
    },
    "pagestyle": {
      "category": "actor",
      "typeName": "pagestyle",
      "methods": [
        {
          "name": "getComputed",
          "request": {
            "type": "getComputed",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "markMatched": {
              "_option": 1,
              "type": "boolean"
            },
            "onlyMatched": {
              "_option": 1,
              "type": "boolean"
            },
            "filter": {
              "_option": 1,
              "type": "string"
            }
          },
          "response": {
            "computed": {
              "_retval": "json"
            }
          }
        },
        {
          "name": "getMatchedSelectors",
          "request": {
            "type": "getMatchedSelectors",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "property": {
              "_arg": 1,
              "type": "string"
            },
            "filter": {
              "_option": 2,
              "type": "string"
            }
          },
          "response": {
            "_retval": "matchedselectorresponse"
          }
        },
        {
          "name": "getApplied",
          "request": {
            "type": "getApplied",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "inherited": {
              "_option": 1,
              "type": "boolean"
            },
            "matchedSelectors": {
              "_option": 1,
              "type": "boolean"
            },
            "filter": {
              "_option": 1,
              "type": "string"
            }
          },
          "response": {
            "_retval": "appliedStylesReturn"
          }
        },
        {
          "name": "getLayout",
          "request": {
            "type": "getLayout",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "autoMargins": {
              "_option": 1,
              "type": "boolean"
            }
          },
          "response": {
            "_retval": "json"
          }
        }
      ],
      "events": {}
    },
    "domstylerule": {
      "category": "actor",
      "typeName": "domstylerule",
      "methods": [
        {
          "name": "modifyProperties",
          "request": {
            "type": "modifyProperties",
            "modifications": {
              "_arg": 0,
              "type": "array:json"
            }
          },
          "response": {
            "rule": {
              "_retval": "domstylerule"
            }
          }
        }
      ],
      "events": {}
    },
    "highlighter": {
      "category": "actor",
      "typeName": "highlighter",
      "methods": [
        {
          "name": "showBoxModel",
          "request": {
            "type": "showBoxModel",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "region": {
              "_option": 1,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "hideBoxModel",
          "request": {
            "type": "hideBoxModel"
          },
          "response": {}
        },
        {
          "name": "pick",
          "request": {
            "type": "pick"
          },
          "response": {}
        },
        {
          "name": "cancelPick",
          "request": {
            "type": "cancelPick"
          },
          "response": {}
        }
      ],
      "events": {}
    },
    "imageData": {
      "category": "dict",
      "typeName": "imageData",
      "specializations": {
        "data": "nullable:longstring",
        "size": "json"
      }
    },
    "disconnectedNode": {
      "category": "dict",
      "typeName": "disconnectedNode",
      "specializations": {
        "node": "domnode",
        "newParents": "array:domnode"
      }
    },
    "disconnectedNodeArray": {
      "category": "dict",
      "typeName": "disconnectedNodeArray",
      "specializations": {
        "nodes": "array:domnode",
        "newParents": "array:domnode"
      }
    },
    "dommutation": {
      "category": "dict",
      "typeName": "dommutation",
      "specializations": {}
    },
    "domnodelist": {
      "category": "actor",
      "typeName": "domnodelist",
      "methods": [
        {
          "name": "item",
          "request": {
            "type": "item",
            "item": {
              "_arg": 0,
              "type": "primitive"
            }
          },
          "response": {
            "_retval": "disconnectedNode"
          }
        },
        {
          "name": "items",
          "request": {
            "type": "items",
            "start": {
              "_arg": 0,
              "type": "nullable:number"
            },
            "end": {
              "_arg": 1,
              "type": "nullable:number"
            }
          },
          "response": {
            "_retval": "disconnectedNodeArray"
          }
        },
        {
          "name": "release",
          "release": true,
          "request": {
            "type": "release"
          },
          "response": {}
        }
      ],
      "events": {}
    },
    "domtraversalarray": {
      "category": "dict",
      "typeName": "domtraversalarray",
      "specializations": {
        "nodes": "array:domnode"
      }
    },
    "domwalker": {
      "category": "actor",
      "typeName": "domwalker",
      "methods": [
        {
          "name": "release",
          "release": true,
          "request": {
            "type": "release"
          },
          "response": {}
        },
        {
          "name": "pick",
          "request": {
            "type": "pick"
          },
          "response": {
            "_retval": "disconnectedNode"
          }
        },
        {
          "name": "cancelPick",
          "request": {
            "type": "cancelPick"
          },
          "response": {}
        },
        {
          "name": "highlight",
          "request": {
            "type": "highlight",
            "node": {
              "_arg": 0,
              "type": "nullable:domnode"
            }
          },
          "response": {}
        },
        {
          "name": "document",
          "request": {
            "type": "document",
            "node": {
              "_arg": 0,
              "type": "nullable:domnode"
            }
          },
          "response": {
            "node": {
              "_retval": "domnode"
            }
          }
        },
        {
          "name": "documentElement",
          "request": {
            "type": "documentElement",
            "node": {
              "_arg": 0,
              "type": "nullable:domnode"
            }
          },
          "response": {
            "node": {
              "_retval": "domnode"
            }
          }
        },
        {
          "name": "parents",
          "request": {
            "type": "parents",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "sameDocument": {
              "_option": 1,
              "type": "primitive"
            }
          },
          "response": {
            "nodes": {
              "_retval": "array:domnode"
            }
          }
        },
        {
          "name": "retainNode",
          "request": {
            "type": "retainNode",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {}
        },
        {
          "name": "unretainNode",
          "request": {
            "type": "unretainNode",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {}
        },
        {
          "name": "releaseNode",
          "request": {
            "type": "releaseNode",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "force": {
              "_option": 1,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "children",
          "request": {
            "type": "children",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "maxNodes": {
              "_option": 1,
              "type": "primitive"
            },
            "center": {
              "_option": 1,
              "type": "domnode"
            },
            "start": {
              "_option": 1,
              "type": "domnode"
            },
            "whatToShow": {
              "_option": 1,
              "type": "primitive"
            }
          },
          "response": {
            "_retval": "domtraversalarray"
          }
        },
        {
          "name": "siblings",
          "request": {
            "type": "siblings",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "maxNodes": {
              "_option": 1,
              "type": "primitive"
            },
            "center": {
              "_option": 1,
              "type": "domnode"
            },
            "start": {
              "_option": 1,
              "type": "domnode"
            },
            "whatToShow": {
              "_option": 1,
              "type": "primitive"
            }
          },
          "response": {
            "_retval": "domtraversalarray"
          }
        },
        {
          "name": "nextSibling",
          "request": {
            "type": "nextSibling",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "whatToShow": {
              "_option": 1,
              "type": "primitive"
            }
          },
          "response": {
            "node": {
              "_retval": "nullable:domnode"
            }
          }
        },
        {
          "name": "previousSibling",
          "request": {
            "type": "previousSibling",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "whatToShow": {
              "_option": 1,
              "type": "primitive"
            }
          },
          "response": {
            "node": {
              "_retval": "nullable:domnode"
            }
          }
        },
        {
          "name": "querySelector",
          "request": {
            "type": "querySelector",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "selector": {
              "_arg": 1,
              "type": "primitive"
            }
          },
          "response": {
            "_retval": "disconnectedNode"
          }
        },
        {
          "name": "querySelectorAll",
          "request": {
            "type": "querySelectorAll",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "selector": {
              "_arg": 1,
              "type": "primitive"
            }
          },
          "response": {
            "list": {
              "_retval": "domnodelist"
            }
          }
        },
        {
          "name": "getSuggestionsForQuery",
          "request": {
            "type": "getSuggestionsForQuery",
            "query": {
              "_arg": 0,
              "type": "primitive"
            },
            "completing": {
              "_arg": 1,
              "type": "primitive"
            },
            "selectorState": {
              "_arg": 2,
              "type": "primitive"
            }
          },
          "response": {
            "list": {
              "_retval": "array:array:string"
            }
          }
        },
        {
          "name": "addPseudoClassLock",
          "request": {
            "type": "addPseudoClassLock",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "pseudoClass": {
              "_arg": 1,
              "type": "primitive"
            },
            "parents": {
              "_option": 2,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "hideNode",
          "request": {
            "type": "hideNode",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {}
        },
        {
          "name": "unhideNode",
          "request": {
            "type": "unhideNode",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {}
        },
        {
          "name": "removePseudoClassLock",
          "request": {
            "type": "removePseudoClassLock",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "pseudoClass": {
              "_arg": 1,
              "type": "primitive"
            },
            "parents": {
              "_option": 2,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "clearPseudoClassLocks",
          "request": {
            "type": "clearPseudoClassLocks",
            "node": {
              "_arg": 0,
              "type": "nullable:domnode"
            }
          },
          "response": {}
        },
        {
          "name": "innerHTML",
          "request": {
            "type": "innerHTML",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {
            "value": {
              "_retval": "longstring"
            }
          }
        },
        {
          "name": "outerHTML",
          "request": {
            "type": "outerHTML",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {
            "value": {
              "_retval": "longstring"
            }
          }
        },
        {
          "name": "setOuterHTML",
          "request": {
            "type": "setOuterHTML",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "value": {
              "_arg": 1,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "removeNode",
          "request": {
            "type": "removeNode",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {
            "nextSibling": {
              "_retval": "nullable:domnode"
            }
          }
        },
        {
          "name": "insertBefore",
          "request": {
            "type": "insertBefore",
            "node": {
              "_arg": 0,
              "type": "domnode"
            },
            "parent": {
              "_arg": 1,
              "type": "domnode"
            },
            "sibling": {
              "_arg": 2,
              "type": "nullable:domnode"
            }
          },
          "response": {}
        },
        {
          "name": "getMutations",
          "request": {
            "type": "getMutations",
            "cleanup": {
              "_option": 0,
              "type": "primitive"
            }
          },
          "response": {
            "mutations": {
              "_retval": "array:dommutation"
            }
          }
        },
        {
          "name": "isInDOMTree",
          "request": {
            "type": "isInDOMTree",
            "node": {
              "_arg": 0,
              "type": "domnode"
            }
          },
          "response": {
            "attached": {
              "_retval": "boolean"
            }
          }
        },
        {
          "name": "getNodeActorFromObjectActor",
          "request": {
            "type": "getNodeActorFromObjectActor",
            "objectActorID": {
              "_arg": 0,
              "type": "string"
            }
          },
          "response": {
            "nodeFront": {
              "_retval": "nullable:disconnectedNode"
            }
          }
        }
      ],
      "events": {
        "new-mutations": {
          "type": "newMutations"
        },
        "picker-node-picked": {
          "type": "pickerNodePicked",
          "node": {
            "_arg": 0,
            "type": "disconnectedNode"
          }
        },
        "picker-node-hovered": {
          "type": "pickerNodeHovered",
          "node": {
            "_arg": 0,
            "type": "disconnectedNode"
          }
        },
        "highlighter-ready": {
          "type": "highlighter-ready"
        },
        "highlighter-hide": {
          "type": "highlighter-hide"
        }
      }
    },
    "inspector": {
      "category": "actor",
      "typeName": "inspector",
      "methods": [
        {
          "name": "getWalker",
          "request": {
            "type": "getWalker"
          },
          "response": {
            "walker": {
              "_retval": "domwalker"
            }
          }
        },
        {
          "name": "getPageStyle",
          "request": {
            "type": "getPageStyle"
          },
          "response": {
            "pageStyle": {
              "_retval": "pagestyle"
            }
          }
        },
        {
          "name": "getHighlighter",
          "request": {
            "type": "getHighlighter",
            "autohide": {
              "_arg": 0,
              "type": "boolean"
            }
          },
          "response": {
            "highligter": {
              "_retval": "highlighter"
            }
          }
        },
        {
          "name": "getImageDataFromURL",
          "request": {
            "type": "getImageDataFromURL",
            "url": {
              "_arg": 0,
              "type": "primitive"
            },
            "maxDim": {
              "_arg": 1,
              "type": "nullable:number"
            }
          },
          "response": {
            "_retval": "imageData"
          }
        }
      ],
      "events": {}
    },
    "call-stack-item": {
      "category": "dict",
      "typeName": "call-stack-item",
      "specializations": {
        "name": "string",
        "file": "string",
        "line": "number"
      }
    },
    "call-details": {
      "category": "dict",
      "typeName": "call-details",
      "specializations": {
        "type": "number",
        "name": "string",
        "stack": "array:call-stack-item"
      }
    },
    "function-call": {
      "category": "actor",
      "typeName": "function-call",
      "methods": [
        {
          "name": "getDetails",
          "request": {
            "type": "getDetails"
          },
          "response": {
            "info": {
              "_retval": "call-details"
            }
          }
        }
      ],
      "events": {}
    },
    "call-watcher": {
      "category": "actor",
      "typeName": "call-watcher",
      "methods": [
        {
          "name": "setup",
          "oneway": true,
          "request": {
            "type": "setup",
            "tracedGlobals": {
              "_option": 0,
              "type": "nullable:array:string"
            },
            "tracedFunctions": {
              "_option": 0,
              "type": "nullable:array:string"
            },
            "startRecording": {
              "_option": 0,
              "type": "boolean"
            },
            "performReload": {
              "_option": 0,
              "type": "boolean"
            }
          },
          "response": {}
        },
        {
          "name": "finalize",
          "oneway": true,
          "request": {
            "type": "finalize"
          },
          "response": {}
        },
        {
          "name": "isRecording",
          "request": {
            "type": "isRecording"
          },
          "response": {
            "_retval": "boolean"
          }
        },
        {
          "name": "resumeRecording",
          "request": {
            "type": "resumeRecording"
          },
          "response": {}
        },
        {
          "name": "pauseRecording",
          "request": {
            "type": "pauseRecording"
          },
          "response": {
            "calls": {
              "_retval": "array:function-call"
            }
          }
        },
        {
          "name": "eraseRecording",
          "request": {
            "type": "eraseRecording"
          },
          "response": {}
        }
      ],
      "events": {}
    },
    "snapshot-image": {
      "category": "dict",
      "typeName": "snapshot-image",
      "specializations": {
        "index": "number",
        "width": "number",
        "height": "number",
        "flipped": "boolean",
        "pixels": "uint32-array"
      }
    },
    "snapshot-overview": {
      "category": "dict",
      "typeName": "snapshot-overview",
      "specializations": {
        "calls": "array:function-call",
        "thumbnails": "array:snapshot-image",
        "screenshot": "snapshot-image"
      }
    },
    "frame-snapshot": {
      "category": "actor",
      "typeName": "frame-snapshot",
      "methods": [
        {
          "name": "getOverview",
          "request": {
            "type": "getOverview"
          },
          "response": {
            "overview": {
              "_retval": "snapshot-overview"
            }
          }
        },
        {
          "name": "generateScreenshotFor",
          "request": {
            "type": "generateScreenshotFor",
            "call": {
              "_arg": 0,
              "type": "function-call"
            }
          },
          "response": {
            "screenshot": {
              "_retval": "snapshot-image"
            }
          }
        }
      ],
      "events": {}
    },
    "canvas": {
      "category": "actor",
      "typeName": "canvas",
      "methods": [
        {
          "name": "setup",
          "oneway": true,
          "request": {
            "type": "setup",
            "reload": {
              "_option": 0,
              "type": "boolean"
            }
          },
          "response": {}
        },
        {
          "name": "finalize",
          "oneway": true,
          "request": {
            "type": "finalize"
          },
          "response": {}
        },
        {
          "name": "isInitialized",
          "request": {
            "type": "isInitialized"
          },
          "response": {
            "initialized": {
              "_retval": "boolean"
            }
          }
        },
        {
          "name": "recordAnimationFrame",
          "request": {
            "type": "recordAnimationFrame"
          },
          "response": {
            "snapshot": {
              "_retval": "frame-snapshot"
            }
          }
        }
      ],
      "events": {}
    },
    "gl-shader": {
      "category": "actor",
      "typeName": "gl-shader",
      "methods": [
        {
          "name": "getText",
          "request": {
            "type": "getText"
          },
          "response": {
            "text": {
              "_retval": "string"
            }
          }
        },
        {
          "name": "compile",
          "request": {
            "type": "compile",
            "text": {
              "_arg": 0,
              "type": "string"
            }
          },
          "response": {
            "error": {
              "_retval": "nullable:json"
            }
          }
        }
      ],
      "events": {}
    },
    "gl-program": {
      "category": "actor",
      "typeName": "gl-program",
      "methods": [
        {
          "name": "getVertexShader",
          "request": {
            "type": "getVertexShader"
          },
          "response": {
            "shader": {
              "_retval": "gl-shader"
            }
          }
        },
        {
          "name": "getFragmentShader",
          "request": {
            "type": "getFragmentShader"
          },
          "response": {
            "shader": {
              "_retval": "gl-shader"
            }
          }
        },
        {
          "name": "highlight",
          "oneway": true,
          "request": {
            "type": "highlight",
            "tint": {
              "_arg": 0,
              "type": "array:number"
            }
          },
          "response": {}
        },
        {
          "name": "unhighlight",
          "oneway": true,
          "request": {
            "type": "unhighlight"
          },
          "response": {}
        },
        {
          "name": "blackbox",
          "oneway": true,
          "request": {
            "type": "blackbox"
          },
          "response": {}
        },
        {
          "name": "unblackbox",
          "oneway": true,
          "request": {
            "type": "unblackbox"
          },
          "response": {}
        }
      ],
      "events": {}
    },
    "webgl": {
      "category": "actor",
      "typeName": "webgl",
      "methods": [
        {
          "name": "setup",
          "oneway": true,
          "request": {
            "type": "setup",
            "reload": {
              "_option": 0,
              "type": "boolean"
            }
          },
          "response": {}
        },
        {
          "name": "finalize",
          "oneway": true,
          "request": {
            "type": "finalize"
          },
          "response": {}
        },
        {
          "name": "getPrograms",
          "request": {
            "type": "getPrograms"
          },
          "response": {
            "programs": {
              "_retval": "array:gl-program"
            }
          }
        }
      ],
      "events": {
        "program-linked": {
          "type": "programLinked",
          "program": {
            "_arg": 0,
            "type": "gl-program"
          }
        }
      }
    },
    "audionode": {
      "category": "actor",
      "typeName": "audionode",
      "methods": [
        {
          "name": "getType",
          "request": {
            "type": "getType"
          },
          "response": {
            "type": {
              "_retval": "string"
            }
          }
        },
        {
          "name": "isSource",
          "request": {
            "type": "isSource"
          },
          "response": {
            "source": {
              "_retval": "boolean"
            }
          }
        },
        {
          "name": "setParam",
          "request": {
            "type": "setParam",
            "param": {
              "_arg": 0,
              "type": "string"
            },
            "value": {
              "_arg": 1,
              "type": "nullable:primitive"
            }
          },
          "response": {
            "error": {
              "_retval": "nullable:json"
            }
          }
        },
        {
          "name": "getParam",
          "request": {
            "type": "getParam",
            "param": {
              "_arg": 0,
              "type": "string"
            }
          },
          "response": {
            "text": {
              "_retval": "nullable:primitive"
            }
          }
        },
        {
          "name": "getParamFlags",
          "request": {
            "type": "getParamFlags",
            "param": {
              "_arg": 0,
              "type": "string"
            }
          },
          "response": {
            "flags": {
              "_retval": "nullable:primitive"
            }
          }
        },
        {
          "name": "getParams",
          "request": {
            "type": "getParams"
          },
          "response": {
            "params": {
              "_retval": "json"
            }
          }
        }
      ],
      "events": {}
    },
    "webaudio": {
      "category": "actor",
      "typeName": "webaudio",
      "methods": [
        {
          "name": "setup",
          "oneway": true,
          "request": {
            "type": "setup",
            "reload": {
              "_option": 0,
              "type": "boolean"
            }
          },
          "response": {}
        },
        {
          "name": "finalize",
          "oneway": true,
          "request": {
            "type": "finalize"
          },
          "response": {}
        }
      ],
      "events": {
        "start-context": {
          "type": "startContext"
        },
        "connect-node": {
          "type": "connectNode",
          "source": {
            "_option": 0,
            "type": "audionode"
          },
          "dest": {
            "_option": 0,
            "type": "audionode"
          }
        },
        "disconnect-node": {
          "type": "disconnectNode",
          "source": {
            "_arg": 0,
            "type": "audionode"
          }
        },
        "connect-param": {
          "type": "connectParam",
          "source": {
            "_arg": 0,
            "type": "audionode"
          },
          "param": {
            "_arg": 1,
            "type": "string"
          }
        },
        "change-param": {
          "type": "changeParam",
          "source": {
            "_option": 0,
            "type": "audionode"
          },
          "param": {
            "_option": 0,
            "type": "string"
          },
          "value": {
            "_option": 0,
            "type": "string"
          }
        },
        "create-node": {
          "type": "createNode",
          "source": {
            "_arg": 0,
            "type": "audionode"
          }
        }
      }
    },
    "old-stylesheet": {
      "category": "actor",
      "typeName": "old-stylesheet",
      "methods": [
        {
          "name": "toggleDisabled",
          "request": {
            "type": "toggleDisabled"
          },
          "response": {
            "disabled": {
              "_retval": "boolean"
            }
          }
        },
        {
          "name": "fetchSource",
          "request": {
            "type": "fetchSource"
          },
          "response": {}
        },
        {
          "name": "update",
          "request": {
            "type": "update",
            "text": {
              "_arg": 0,
              "type": "string"
            },
            "transition": {
              "_arg": 1,
              "type": "boolean"
            }
          },
          "response": {}
        }
      ],
      "events": {
        "property-change": {
          "type": "propertyChange",
          "property": {
            "_arg": 0,
            "type": "string"
          },
          "value": {
            "_arg": 1,
            "type": "json"
          }
        },
        "source-load": {
          "type": "sourceLoad",
          "source": {
            "_arg": 0,
            "type": "string"
          }
        },
        "style-applied": {
          "type": "styleApplied"
        }
      }
    },
    "styleeditor": {
      "category": "actor",
      "typeName": "styleeditor",
      "methods": [
        {
          "name": "newDocument",
          "request": {
            "type": "newDocument"
          },
          "response": {}
        },
        {
          "name": "newStyleSheet",
          "request": {
            "type": "newStyleSheet",
            "text": {
              "_arg": 0,
              "type": "string"
            }
          },
          "response": {
            "styleSheet": {
              "_retval": "old-stylesheet"
            }
          }
        }
      ],
      "events": {
        "document-load": {
          "type": "documentLoad",
          "styleSheets": {
            "_arg": 0,
            "type": "array:old-stylesheet"
          }
        }
      }
    },
    "cookieobject": {
      "category": "dict",
      "typeName": "cookieobject",
      "specializations": {
        "name": "string",
        "value": "longstring",
        "path": "nullable:string",
        "host": "string",
        "isDomain": "boolean",
        "isSecure": "boolean",
        "isHttpOnly": "boolean",
        "creationTime": "number",
        "lastAccessed": "number",
        "expires": "number"
      }
    },
    "cookiestoreobject": {
      "category": "dict",
      "typeName": "cookiestoreobject",
      "specializations": {
        "total": "number",
        "offset": "number",
        "data": "array:nullable:cookieobject"
      }
    },
    "storageobject": {
      "category": "dict",
      "typeName": "storageobject",
      "specializations": {
        "name": "string",
        "value": "longstring"
      }
    },
    "storagestoreobject": {
      "category": "dict",
      "typeName": "storagestoreobject",
      "specializations": {
        "total": "number",
        "offset": "number",
        "data": "array:nullable:storageobject"
      }
    },
    "idbobject": {
      "category": "dict",
      "typeName": "idbobject",
      "specializations": {
        "name": "nullable:string",
        "db": "nullable:string",
        "objectStore": "nullable:string",
        "origin": "nullable:string",
        "version": "nullable:number",
        "objectStores": "nullable:number",
        "keyPath": "nullable:string",
        "autoIncrement": "nullable:boolean",
        "indexes": "nullable:string",
        "value": "nullable:longstring"
      }
    },
    "idbstoreobject": {
      "category": "dict",
      "typeName": "idbstoreobject",
      "specializations": {
        "total": "number",
        "offset": "number",
        "data": "array:nullable:idbobject"
      }
    },
    "storeUpdateObject": {
      "category": "dict",
      "typeName": "storeUpdateObject",
      "specializations": {
        "changed": "nullable:json",
        "deleted": "nullable:json",
        "added": "nullable:json"
      }
    },
    "cookies": {
      "category": "actor",
      "typeName": "cookies",
      "methods": [
        {
          "name": "getStoreObjects",
          "request": {
            "type": "getStoreObjects",
            "host": {
              "_arg": 0,
              "type": "primitive"
            },
            "names": {
              "_arg": 1,
              "type": "nullable:array:string"
            },
            "options": {
              "_arg": 2,
              "type": "nullable:json"
            }
          },
          "response": {
            "_retval": "cookiestoreobject"
          }
        }
      ],
      "events": {}
    },
    "localStorage": {
      "category": "actor",
      "typeName": "localStorage",
      "methods": [
        {
          "name": "getStoreObjects",
          "request": {
            "type": "getStoreObjects",
            "host": {
              "_arg": 0,
              "type": "primitive"
            },
            "names": {
              "_arg": 1,
              "type": "nullable:array:string"
            },
            "options": {
              "_arg": 2,
              "type": "nullable:json"
            }
          },
          "response": {
            "_retval": "storagestoreobject"
          }
        }
      ],
      "events": {}
    },
    "sessionStorage": {
      "category": "actor",
      "typeName": "sessionStorage",
      "methods": [
        {
          "name": "getStoreObjects",
          "request": {
            "type": "getStoreObjects",
            "host": {
              "_arg": 0,
              "type": "primitive"
            },
            "names": {
              "_arg": 1,
              "type": "nullable:array:string"
            },
            "options": {
              "_arg": 2,
              "type": "nullable:json"
            }
          },
          "response": {
            "_retval": "storagestoreobject"
          }
        }
      ],
      "events": {}
    },
    "indexedDB": {
      "category": "actor",
      "typeName": "indexedDB",
      "methods": [
        {
          "name": "getStoreObjects",
          "request": {
            "type": "getStoreObjects",
            "host": {
              "_arg": 0,
              "type": "primitive"
            },
            "names": {
              "_arg": 1,
              "type": "nullable:array:string"
            },
            "options": {
              "_arg": 2,
              "type": "nullable:json"
            }
          },
          "response": {
            "_retval": "idbstoreobject"
          }
        }
      ],
      "events": {}
    },
    "storelist": {
      "category": "dict",
      "typeName": "storelist",
      "specializations": {
        "cookies": "cookies",
        "localStorage": "localStorage",
        "sessionStorage": "sessionStorage",
        "indexedDB": "indexedDB"
      }
    },
    "storage": {
      "category": "actor",
      "typeName": "storage",
      "methods": [
        {
          "name": "listStores",
          "request": {
            "type": "listStores"
          },
          "response": {
            "_retval": "storelist"
          }
        }
      ],
      "events": {
        "stores-update": {
          "type": "storesUpdate",
          "data": {
            "_arg": 0,
            "type": "storeUpdateObject"
          }
        },
        "stores-cleared": {
          "type": "storesCleared",
          "data": {
            "_arg": 0,
            "type": "json"
          }
        },
        "stores-reloaded": {
          "type": "storesRelaoded",
          "data": {
            "_arg": 0,
            "type": "json"
          }
        }
      }
    },
    "gcli": {
      "category": "actor",
      "typeName": "gcli",
      "methods": [
        {
          "name": "specs",
          "request": {
            "type": "specs"
          },
          "response": {
            "_retval": "json"
          }
        },
        {
          "name": "execute",
          "request": {
            "type": "execute",
            "typed": {
              "_arg": 0,
              "type": "string"
            }
          },
          "response": {
            "_retval": "json"
          }
        },
        {
          "name": "state",
          "request": {
            "type": "state",
            "typed": {
              "_arg": 0,
              "type": "string"
            },
            "start": {
              "_arg": 1,
              "type": "number"
            },
            "rank": {
              "_arg": 2,
              "type": "number"
            }
          },
          "response": {
            "_retval": "json"
          }
        },
        {
          "name": "typeparse",
          "request": {
            "type": "typeparse",
            "typed": {
              "_arg": 0,
              "type": "string"
            },
            "param": {
              "_arg": 1,
              "type": "string"
            }
          },
          "response": {
            "_retval": "json"
          }
        },
        {
          "name": "typeincrement",
          "request": {
            "type": "typeincrement",
            "typed": {
              "_arg": 0,
              "type": "string"
            },
            "param": {
              "_arg": 1,
              "type": "string"
            }
          },
          "response": {
            "_retval": "string"
          }
        },
        {
          "name": "typedecrement",
          "request": {
            "type": "typedecrement",
            "typed": {
              "_arg": 0,
              "type": "string"
            },
            "param": {
              "_arg": 1,
              "type": "string"
            }
          },
          "response": {
            "_retval": "string"
          }
        },
        {
          "name": "selectioninfo",
          "request": {
            "type": "selectioninfo",
            "typed": {
              "_arg": 0,
              "type": "string"
            },
            "param": {
              "_arg": 1,
              "type": "string"
            },
            "action": {
              "_arg": 1,
              "type": "string"
            }
          },
          "response": {
            "_retval": "json"
          }
        }
      ],
      "events": {}
    },
    "memory": {
      "category": "actor",
      "typeName": "memory",
      "methods": [
        {
          "name": "measure",
          "request": {
            "type": "measure"
          },
          "response": {
            "_retval": "json"
          }
        }
      ],
      "events": {}
    },
    "eventLoopLag": {
      "category": "actor",
      "typeName": "eventLoopLag",
      "methods": [
        {
          "name": "start",
          "request": {
            "type": "start"
          },
          "response": {
            "success": {
              "_retval": "number"
            }
          }
        },
        {
          "name": "stop",
          "request": {
            "type": "stop"
          },
          "response": {}
        }
      ],
      "events": {
        "event-loop-lag": {
          "type": "event-loop-lag",
          "time": {
            "_arg": 0,
            "type": "number"
          }
        }
      }
    },
    "preference": {
      "category": "actor",
      "typeName": "preference",
      "methods": [
        {
          "name": "getBoolPref",
          "request": {
            "type": "getBoolPref",
            "value": {
              "_arg": 0,
              "type": "primitive"
            }
          },
          "response": {
            "value": {
              "_retval": "boolean"
            }
          }
        },
        {
          "name": "getCharPref",
          "request": {
            "type": "getCharPref",
            "value": {
              "_arg": 0,
              "type": "primitive"
            }
          },
          "response": {
            "value": {
              "_retval": "string"
            }
          }
        },
        {
          "name": "getIntPref",
          "request": {
            "type": "getIntPref",
            "value": {
              "_arg": 0,
              "type": "primitive"
            }
          },
          "response": {
            "value": {
              "_retval": "number"
            }
          }
        },
        {
          "name": "getAllPrefs",
          "request": {
            "type": "getAllPrefs"
          },
          "response": {
            "value": {
              "_retval": "json"
            }
          }
        },
        {
          "name": "setBoolPref",
          "request": {
            "type": "setBoolPref",
            "name": {
              "_arg": 0,
              "type": "primitive"
            },
            "value": {
              "_arg": 1,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "setCharPref",
          "request": {
            "type": "setCharPref",
            "name": {
              "_arg": 0,
              "type": "primitive"
            },
            "value": {
              "_arg": 1,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "setIntPref",
          "request": {
            "type": "setIntPref",
            "name": {
              "_arg": 0,
              "type": "primitive"
            },
            "value": {
              "_arg": 1,
              "type": "primitive"
            }
          },
          "response": {}
        },
        {
          "name": "clearUserPref",
          "request": {
            "type": "clearUserPref",
            "name": {
              "_arg": 0,
              "type": "primitive"
            }
          },
          "response": {}
        }
      ],
      "events": {}
    },
    "device": {
      "category": "actor",
      "typeName": "device",
      "methods": [
        {
          "name": "getDescription",
          "request": {
            "type": "getDescription"
          },
          "response": {
            "value": {
              "_retval": "json"
            }
          }
        },
        {
          "name": "getWallpaper",
          "request": {
            "type": "getWallpaper"
          },
          "response": {
            "value": {
              "_retval": "longstring"
            }
          }
        },
        {
          "name": "screenshotToDataURL",
          "request": {
            "type": "screenshotToDataURL"
          },
          "response": {
            "value": {
              "_retval": "longstring"
            }
          }
        },
        {
          "name": "getRawPermissionsTable",
          "request": {
            "type": "getRawPermissionsTable"
          },
          "response": {
            "value": {
              "_retval": "json"
            }
          }
        }
      ],
      "events": {}
    }
  },
  "from": "root"
}

},{}],25:[function(_dereq_,module,exports){
"use strict";

var Class = _dereq_("./class").Class;
var util = _dereq_("./util");
var keys = util.keys;
var values = util.values;
var pairs = util.pairs;
var query = util.query;
var findPath = util.findPath;
var EventTarget = _dereq_("./event").EventTarget;

var TypeSystem = Class({
  constructor: function(client) {
    var types = Object.create(null);
    var specification = Object.create(null);

    this.specification = specification;
    this.types = types;

    var typeFor = function typeFor(typeName) {
      typeName = typeName || "primitive";
      if (!types[typeName]) {
        defineType(typeName);
      }

      return types[typeName];
    };
    this.typeFor = typeFor;

    var defineType = function(descriptor) {
      var type = void(0);
      if (typeof(descriptor) === "string") {
        if (descriptor.indexOf(":") > 0)
          type = makeCompoundType(descriptor);
        else if (descriptor.indexOf("#") > 0)
          type = new ActorDetail(descriptor);
          else if (specification[descriptor])
            type = makeCategoryType(specification[descriptor]);
      } else {
        type = makeCategoryType(descriptor);
      }

      if (type)
        types[type.name] = type;
      else
        throw TypeError("Invalid type: " + descriptor);
    };
    this.defineType = defineType;


    var makeCompoundType = function(name) {
      var index = name.indexOf(":");
      var baseType = name.slice(0, index);
      var subType = name.slice(index + 1);

      return baseType === "array" ? new ArrayOf(subType) :
      baseType === "nullable" ? new Maybe(subType) :
      null;
    };

    var makeCategoryType = function(descriptor) {
      var category = descriptor.category;
      return category === "dict" ? new Dictionary(descriptor) :
      category === "actor" ? new Actor(descriptor) :
      null;
    };

    var read = function(input, context, typeName) {
      return typeFor(typeName).read(input, context);
    }
    this.read = read;

    var write = function(input, context, typeName) {
      return typeFor(typeName).write(input);
    };
    this.write = write;


    var Type = Class({
      constructor: function() {
      },
      get name() {
        return this.category ? this.category + ":" + this.type :
        this.type;
      },
      read: function(input, context) {
        throw new TypeError("`Type` subclass must implement `read`");
      },
      write: function(input, context) {
        throw new TypeError("`Type` subclass must implement `write`");
      }
    });

    var Primitve = Class({
      extends: Type,
      constuctor: function(type) {
        this.type = type;
      },
      read: function(input, context) {
        return input;
      },
      write: function(input, context) {
        return input;
      }
    });

    var Maybe = Class({
      extends: Type,
      category: "nullable",
      constructor: function(type) {
        this.type = type;
      },
      read: function(input, context) {
        return input === null ? null :
        input === void(0) ? void(0) :
        read(input, context, this.type);
      },
      write: function(input, context) {
        return input === null ? null :
        input === void(0) ? void(0) :
        write(input, context, this.type);
      }
    });

    var ArrayOf = Class({
      extends: Type,
      category: "array",
      constructor: function(type) {
        this.type = type;
      },
      read: function(input, context) {
        var type = this.type;
        return input.map(function($) { return read($, context, type) });
      },
      write: function(input, context) {
        var type = this.type;
        return input.map(function($) { return write($, context, type) });
      }
    });

    var makeField = function makeField(name, type) {
      return {
        enumerable: true,
        configurable: true,
        get: function() {
          Object.defineProperty(this, name, {
            configurable: false,
            value: read(this.state[name], this.context, type)
          });
          return this[name];
        }
      }
    };

    var makeFields = function(descriptor) {
      return pairs(descriptor).reduce(function(fields, pair) {
        var name = pair[0], type = pair[1];
        fields[name] = makeField(name, type);
        return fields;
      }, {});
    }

    var DictionaryType = Class({});

    var Dictionary = Class({
      extends: Type,
      category: "dict",
      get name() { return this.type; },
      constructor: function(descriptor) {
        this.type = descriptor.typeName;
        this.types = descriptor.specializations;

        var proto = Object.defineProperties({
          extends: DictionaryType,
          constructor: function(state, context) {
            Object.defineProperties(this, {
              state: {
                enumerable: false,
                writable: true,
                configurable: true,
                value: state
              },
              context: {
                enumerable: false,
                writable: false,
                configurable: true,
                value: context
              }
            });
          }
        }, makeFields(this.types));

        this.class = new Class(proto);
      },
      read: function(input, context) {
        return new this.class(input, context);
      },
      write: function(input, context) {
        var output = {};
        for (var key in input) {
          output[key] = write(value, context, types[key]);
        }
        return output;
      }
    });

    var makeMethods = function(descriptors) {
      return descriptors.reduce(function(methods, descriptor) {
        methods[descriptor.name] = {
          enumerable: true,
          configurable: true,
          writable: false,
          value: makeMethod(descriptor)
        };
        return methods;
      }, {});
    };

    var makeEvents = function(descriptors) {
      return pairs(descriptors).reduce(function(events, pair) {
        var name = pair[0], descriptor = pair[1];
        var event = new Event(name, descriptor);
        events[event.eventType] = event;
        return events;
      }, Object.create(null));
    };

    var Actor = Class({
      extends: Type,
      category: "actor",
      get name() { return this.type; },
      constructor: function(descriptor) {
        this.type = descriptor.typeName;

        var events = makeEvents(descriptor.events || {});
        var fields = makeFields(descriptor.fields || {});
        var methods = makeMethods(descriptor.methods || []);


        var proto = {
          extends: Front,
          constructor: function() {
            Front.apply(this, arguments);
          },
          events: events
        };
        Object.defineProperties(proto, fields);
        Object.defineProperties(proto, methods);

        this.class = Class(proto);
      },
      read: function(input, context, detail) {
        var state = typeof(input) === "string" ? { actor: input } : input;

        var actor = client.get(state.actor) || new this.class(state, context);
        actor.form(state, detail, context);

        return actor;
      },
      write: function(input, context, detail) {
        return input.id;
      }
    });
    exports.Actor = Actor;


    var ActorDetail = Class({
      extends: Actor,
      constructor: function(name) {
        var parts = name.split("#")
        this.actorType = parts[0]
        this.detail = parts[1];
      },
      read: function(input, context) {
        return typeFor(this.actorType).read(input, context, this.detail);
      },
      write: function(input, context) {
        return typeFor(this.actorType).write(input, context, this.detail);
      }
    });
    exports.ActorDetail = ActorDetail;

    var Method = Class({
      extends: Type,
      constructor: function(descriptor) {
        this.type = descriptor.name;
        this.path = findPath(descriptor.response, "_retval");
        this.responseType = this.path && query(descriptor.response, this.path)._retval;
        this.requestType = descriptor.request.type;

        var params = [];
        for (var key in descriptor.request) {
          if (key !== "type") {
            var param = descriptor.request[key];
            var index = "_arg" in param ? param._arg : param._option;
            var isParam = param._option === index;
            var isArgument = param._arg === index;
            params[index] = {
              type: param.type,
              key: key,
              index: index,
              isParam: isParam,
              isArgument: isArgument
            };
          }
        }
        this.params = params;
      },
      read: function(input, context) {
        return read(query(input, this.path), context, this.responseType);
      },
      write: function(input, context) {
        return this.params.reduce(function(result, param) {
          result[param.key] = write(input[param.index], context, param.type);
          return result;
        }, {type: this.type});
      }
    });
    exports.Method = Method;

    var profiler = function(method, id) {
      return function() {
        var start = new Date();
        return method.apply(this, arguments).then(function(result) {
          var end = new Date();
          client.telemetry.add(id, +end - start);
          return result;
        });
      };
    };

    var destructor = function(method) {
      return function() {
        return method.apply(this, arguments).then(function(result) {
          client.release(this);
          return result;
        });
      };
    };

    function makeMethod(descriptor) {
      var type = new Method(descriptor);
      var method = descriptor.oneway ? makeUnidirecationalMethod(descriptor, type) :
                   makeBidirectionalMethod(descriptor, type);

      if (descriptor.telemetry)
        method = profiler(method);
      if (descriptor.release)
        method = destructor(method);

      return method;
    }

    var makeUnidirecationalMethod = function(descriptor, type) {
      return function() {
        var packet = type.write(arguments, this);
        packet.to = this.id;
        client.send(packet);
        return Promise.resolve(void(0));
      };
    };

    var makeBidirectionalMethod = function(descriptor, type) {
      return function() {
        var context = this.context;
        var packet = type.write(arguments, context);
        var context = this.context;
        packet.to = this.id;
        return client.request(packet).then(function(packet) {
          return type.read(packet, context);
        });
      };
    };

    var Event = Class({
      constructor: function(name, descriptor) {
        this.name = descriptor.type || name;
        this.eventType = descriptor.type || name;
        this.types = Object.create(null);

        var types = this.types;
        for (var key in descriptor) {
          if (key === "type") {
            types[key] = "string";
          } else {
            types[key] = descriptor[key].type;
          }
        }
      },
      read: function(input, context) {
        var output = {};
        var types = this.types;
        for (var key in input) {
          output[key] = read(input[key], context, types[key]);
        }
        return output;
      },
      write: function(input, context) {
        var output = {};
        var types = this.types;
        for (var key in this.types) {
          output[key] = write(input[key], context, types[key]);
        }
        return output;
      }
    });

    var Front = Class({
      extends: EventTarget,
      EventTarget: EventTarget,
      constructor: function(state) {
        this.EventTarget();
        Object.defineProperties(this,  {
          state: {
            enumerable: false,
            writable: true,
            configurable: true,
            value: state
          }
        });

        client.register(this);
      },
      get id() {
        return this.state.actor;
      },
      get context() {
        return this;
      },
      form: function(state, detail, context) {
        if (this.state !== state) {
          if (detail) {
            this.state[detail] = state[detail];
          } else {
            pairs(state).forEach(function(pair) {
              var key = pair[0], value = pair[1];
              this.state[key] = value;
            }, this);
          }
        }

        if (context) {
          client.supervise(context, this);
        }
      },
      requestTypes: function() {
        return client.request({
          to: this.id,
          type: "requestTypes"
        }).then(function(packet) {
          return packet.requestTypes;
        });
      }
    });
    types.primitive = new Primitve("primitive");
    types.string = new Primitve("string");
    types.number = new Primitve("number");
    types.boolean = new Primitve("boolean");
    types.json = new Primitve("json");
    types.array = new Primitve("array");
  },
  registerTypes: function(descriptor) {
    var specification = this.specification;
    values(descriptor.types).forEach(function(descriptor) {
      specification[descriptor.typeName] = descriptor;
    });
  }
});
exports.TypeSystem = TypeSystem;

},{"./class":3,"./event":5,"./util":26}],26:[function(_dereq_,module,exports){
"use strict";

var keys = Object.keys;
exports.keys = keys;

// Returns array of values for the given object.
var values = function(object) {
  return keys(object).map(function(key) {
    return object[key]
  });
};
exports.values = values;

// Returns [key, value] pairs for the given object.
var pairs = function(object) {
  return keys(object).map(function(key) {
    return [key, object[key]]
  });
};
exports.pairs = pairs;


// Queries an object for the field nested with in it.
var query = function(object, path) {
  return path.reduce(function(object, entry) {
    return object && object[entry]
  }, object);
};
exports.query = query;

var isObject = function(x) {
  return x && typeof(x) === "object"
}

var findPath = function(object, key) {
  var path = void(0);
  if (object && typeof(object) === "object") {
    var names = keys(object);
    if (names.indexOf(key) >= 0) {
      path = [];
    } else {
      var index = 0;
      var count = names.length;
      while (index < count && !path){
        var head = names[index];
        var tail = findPath(object[head], key);
        path = tail ? [head].concat(tail) : tail;
        index = index + 1
      }
    }
  }
  return path;
};
exports.findPath = findPath;

},{}]},{},[1])
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZ2VuZXJhdGVkLmpzIiwic291cmNlcyI6WyIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvYnJvd3NlcmlmeS9ub2RlX21vZHVsZXMvYnJvd3Nlci1wYWNrL19wcmVsdWRlLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vYnJvd3Nlci9pbmRleC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL2Jyb3dzZXIvcHJvbWlzZS5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL2NsYXNzLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vY2xpZW50LmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vZXZlbnQuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvYnJvd3NlcmlmeS9ub2RlX21vZHVsZXMvZXZlbnRzL2V2ZW50cy5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL2luZGV4LmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvaXMtaW1wbGVtZW50ZWQuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZC9pbmRleC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL25vZGVfbW9kdWxlcy9lczUtZXh0L29iamVjdC9hc3NpZ24vaW5kZXguanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3QvYXNzaWduL2lzLWltcGxlbWVudGVkLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvb2JqZWN0L2Fzc2lnbi9zaGltLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvb2JqZWN0L2lzLWNhbGxhYmxlLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvb2JqZWN0L2tleXMvaW5kZXguanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3Qva2V5cy9pcy1pbXBsZW1lbnRlZC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL25vZGVfbW9kdWxlcy9lczUtZXh0L29iamVjdC9rZXlzL3NoaW0uanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3Qvbm9ybWFsaXplLW9wdGlvbnMuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3QvdmFsaWQtdmFsdWUuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9zdHJpbmcvIy9jb250YWlucy9pbmRleC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL25vZGVfbW9kdWxlcy9lczUtZXh0L3N0cmluZy8jL2NvbnRhaW5zL2lzLWltcGxlbWVudGVkLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvc3RyaW5nLyMvY29udGFpbnMvc2hpbS5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL3BvbHlmaWxsLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vc3BlY2lmaWNhdGlvbi9jb3JlLmpzb24iLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9zcGVjaWZpY2F0aW9uL3Byb3RvY29sLmpzb24iLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi90eXBlLXN5c3RlbS5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL3V0aWwuanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUE7QUNBQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNUQTtBQUNBO0FBQ0E7QUFDQTs7QUNIQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ3RCQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQzVLQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ25FQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUM1U0E7QUFDQTtBQUNBO0FBQ0E7O0FDSEE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDckJBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQy9EQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDTEE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDVEE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUN0QkE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ0xBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNMQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDUkE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNQQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ3RCQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNOQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDTEE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ1JBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDUEE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ3JEQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQzVFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUMzdUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDcmRBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBIiwic291cmNlc0NvbnRlbnQiOlsiKGZ1bmN0aW9uIGUodCxuLHIpe2Z1bmN0aW9uIHMobyx1KXtpZighbltvXSl7aWYoIXRbb10pe3ZhciBhPXR5cGVvZiByZXF1aXJlPT1cImZ1bmN0aW9uXCImJnJlcXVpcmU7aWYoIXUmJmEpcmV0dXJuIGEobywhMCk7aWYoaSlyZXR1cm4gaShvLCEwKTt0aHJvdyBuZXcgRXJyb3IoXCJDYW5ub3QgZmluZCBtb2R1bGUgJ1wiK28rXCInXCIpfXZhciBmPW5bb109e2V4cG9ydHM6e319O3Rbb11bMF0uY2FsbChmLmV4cG9ydHMsZnVuY3Rpb24oZSl7dmFyIG49dFtvXVsxXVtlXTtyZXR1cm4gcyhuP246ZSl9LGYsZi5leHBvcnRzLGUsdCxuLHIpfXJldHVybiBuW29dLmV4cG9ydHN9dmFyIGk9dHlwZW9mIHJlcXVpcmU9PVwiZnVuY3Rpb25cIiYmcmVxdWlyZTtmb3IodmFyIG89MDtvPHIubGVuZ3RoO28rKylzKHJbb10pO3JldHVybiBzfSkiLCJcInVzZSBzdHJpY3RcIjtcblxudmFyIENsaWVudCA9IHJlcXVpcmUoXCIuLi9jbGllbnRcIikuQ2xpZW50O1xuXG5mdW5jdGlvbiBjb25uZWN0KHBvcnQpIHtcbiAgdmFyIGNsaWVudCA9IG5ldyBDbGllbnQoKTtcbiAgcmV0dXJuIGNsaWVudC5jb25uZWN0KHBvcnQpO1xufVxuZXhwb3J0cy5jb25uZWN0ID0gY29ubmVjdDtcbiIsIlwidXNlIHN0cmljdFwiO1xuXG5leHBvcnRzLlByb21pc2UgPSBQcm9taXNlO1xuIiwiXCJ1c2Ugc3RyaWN0XCI7XG5cbnZhciBkZXNjcmliZSA9IE9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3I7XG52YXIgQ2xhc3MgPSBmdW5jdGlvbihmaWVsZHMpIHtcbiAgdmFyIG5hbWVzID0gT2JqZWN0LmtleXMoZmllbGRzKTtcbiAgdmFyIGNvbnN0cnVjdG9yID0gbmFtZXMuaW5kZXhPZihcImNvbnN0cnVjdG9yXCIpID49IDAgPyBmaWVsZHMuY29uc3RydWN0b3IgOlxuICAgICAgICAgICAgICAgICAgICBmdW5jdGlvbigpIHt9O1xuICB2YXIgYW5jZXN0b3IgPSBmaWVsZHMuZXh0ZW5kcyB8fCBPYmplY3Q7XG5cbiAgdmFyIGRlc2NyaXB0b3IgPSBuYW1lcy5yZWR1Y2UoZnVuY3Rpb24oZGVzY3JpcHRvciwga2V5KSB7XG4gICAgZGVzY3JpcHRvcltrZXldID0gZGVzY3JpYmUoZmllbGRzLCBrZXkpO1xuICAgIHJldHVybiBkZXNjcmlwdG9yO1xuICB9LCB7fSk7XG5cbiAgdmFyIHByb3RvdHlwZSA9IE9iamVjdC5jcmVhdGUoYW5jZXN0b3IucHJvdG90eXBlLCBkZXNjcmlwdG9yKTtcblxuICBjb25zdHJ1Y3Rvci5wcm90b3R5cGUgPSBwcm90b3R5cGU7XG4gIHByb3RvdHlwZS5jb25zdHJ1Y3RvciA9IGNvbnN0cnVjdG9yO1xuXG4gIHJldHVybiBjb25zdHJ1Y3Rvcjtcbn07XG5leHBvcnRzLkNsYXNzID0gQ2xhc3M7XG4iLCJcInVzZSBzdHJpY3RcIjtcblxudmFyIENsYXNzID0gcmVxdWlyZShcIi4vY2xhc3NcIikuQ2xhc3M7XG52YXIgVHlwZVN5c3RlbSA9IHJlcXVpcmUoXCIuL3R5cGUtc3lzdGVtXCIpLlR5cGVTeXN0ZW07XG52YXIgdmFsdWVzID0gcmVxdWlyZShcIi4vdXRpbFwiKS52YWx1ZXM7XG52YXIgUHJvbWlzZSA9IHJlcXVpcmUoXCJlczYtcHJvbWlzZVwiKS5Qcm9taXNlO1xuXG52YXIgc3BlY2lmaWNhdGlvbiA9IHJlcXVpcmUoXCIuL3NwZWNpZmljYXRpb24vY29yZS5qc29uXCIpO1xuXG5mdW5jdGlvbiByZWNvdmVyQWN0b3JEZXNjcmlwdGlvbnMoZXJyb3IpIHtcbiAgY29uc29sZS53YXJuKFwiRmFpbGVkIHRvIGZldGNoIHByb3RvY29sIHNwZWNpZmljYXRpb24gKHNlZSByZWFzb24gYmVsb3cpLiBcIiArXG4gICAgICAgICAgICAgICBcIlVzaW5nIGEgZmFsbGJhY2sgcHJvdG9jYWwgc3BlY2lmaWNhdGlvbiFcIixcbiAgICAgICAgICAgICAgIGVycm9yKTtcbiAgcmV0dXJuIHJlcXVpcmUoXCIuL3NwZWNpZmljYXRpb24vcHJvdG9jb2wuanNvblwiKTtcbn1cblxuLy8gVHlwZSB0byByZXByZXNlbnQgc3VwZXJ2aXNlciBhY3RvciByZWxhdGlvbnMgdG8gYWN0b3JzIHRoZXkgc3VwZXJ2aXNlXG4vLyBpbiB0ZXJtcyBvZiBsaWZldGltZSBtYW5hZ2VtZW50LlxudmFyIFN1cGVydmlzb3IgPSBDbGFzcyh7XG4gIGNvbnN0cnVjdG9yOiBmdW5jdGlvbihpZCkge1xuICAgIHRoaXMuaWQgPSBpZDtcbiAgICB0aGlzLndvcmtlcnMgPSBbXTtcbiAgfVxufSk7XG5cbnZhciBUZWxlbWV0cnkgPSBDbGFzcyh7XG4gIGFkZDogZnVuY3Rpb24oaWQsIG1zKSB7XG4gICAgY29uc29sZS5sb2coXCJ0ZWxlbWV0cnk6OlwiLCBpZCwgbXMpXG4gIH1cbn0pO1xuXG4vLyBDb25zaWRlciBtYWtpbmcgY2xpZW50IGEgcm9vdCBhY3Rvci5cblxudmFyIENsaWVudCA9IENsYXNzKHtcbiAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKCkge1xuICAgIHRoaXMucm9vdCA9IG51bGw7XG4gICAgdGhpcy50ZWxlbWV0cnkgPSBuZXcgVGVsZW1ldHJ5KCk7XG5cbiAgICB0aGlzLnNldHVwQ29ubmVjdGlvbigpO1xuICAgIHRoaXMuc2V0dXBMaWZlTWFuYWdlbWVudCgpO1xuICAgIHRoaXMuc2V0dXBUeXBlU3lzdGVtKCk7XG4gIH0sXG5cbiAgc2V0dXBDb25uZWN0aW9uOiBmdW5jdGlvbigpIHtcbiAgICB0aGlzLnJlcXVlc3RzID0gW107XG4gIH0sXG4gIHNldHVwTGlmZU1hbmFnZW1lbnQ6IGZ1bmN0aW9uKCkge1xuICAgIHRoaXMuY2FjaGUgPSBPYmplY3QuY3JlYXRlKG51bGwpO1xuICAgIHRoaXMuZ3JhcGggPSBPYmplY3QuY3JlYXRlKG51bGwpO1xuICAgIHRoaXMuZ2V0ID0gdGhpcy5nZXQuYmluZCh0aGlzKTtcbiAgICB0aGlzLnJlbGVhc2UgPSB0aGlzLnJlbGVhc2UuYmluZCh0aGlzKTtcbiAgfSxcbiAgc2V0dXBUeXBlU3lzdGVtOiBmdW5jdGlvbigpIHtcbiAgICB0aGlzLnR5cGVTeXN0ZW0gPSBuZXcgVHlwZVN5c3RlbSh0aGlzKTtcbiAgICB0aGlzLnR5cGVTeXN0ZW0ucmVnaXN0ZXJUeXBlcyhzcGVjaWZpY2F0aW9uKTtcbiAgfSxcblxuICBjb25uZWN0OiBmdW5jdGlvbihwb3J0KSB7XG4gICAgdmFyIGNsaWVudCA9IHRoaXM7XG4gICAgcmV0dXJuIG5ldyBQcm9taXNlKGZ1bmN0aW9uKHJlc29sdmUsIHJlamVjdCkge1xuICAgICAgY2xpZW50LnBvcnQgPSBwb3J0O1xuICAgICAgcG9ydC5vbm1lc3NhZ2UgPSBjbGllbnQucmVjZWl2ZS5iaW5kKGNsaWVudCk7XG4gICAgICBjbGllbnQub25SZWFkeSA9IHJlc29sdmU7XG4gICAgICBjbGllbnQub25GYWlsID0gcmVqZWN0O1xuXG4gICAgICBwb3J0LnN0YXJ0KCk7XG4gICAgfSk7XG4gIH0sXG4gIHNlbmQ6IGZ1bmN0aW9uKHBhY2tldCkge1xuICAgIHRoaXMucG9ydC5wb3N0TWVzc2FnZShwYWNrZXQpO1xuICB9LFxuICByZXF1ZXN0OiBmdW5jdGlvbihwYWNrZXQpIHtcbiAgICB2YXIgY2xpZW50ID0gdGhpcztcbiAgICByZXR1cm4gbmV3IFByb21pc2UoZnVuY3Rpb24ocmVzb2x2ZSwgcmVqZWN0KSB7XG4gICAgICBjbGllbnQucmVxdWVzdHMucHVzaChwYWNrZXQudG8sIHsgcmVzb2x2ZTogcmVzb2x2ZSwgcmVqZWN0OiByZWplY3QgfSk7XG4gICAgICBjbGllbnQuc2VuZChwYWNrZXQpO1xuICAgIH0pO1xuICB9LFxuXG4gIHJlY2VpdmU6IGZ1bmN0aW9uKGV2ZW50KSB7XG4gICAgdmFyIHBhY2tldCA9IGV2ZW50LmRhdGE7XG4gICAgaWYgKCF0aGlzLnJvb3QpIHtcbiAgICAgIGlmIChwYWNrZXQuZnJvbSAhPT0gXCJyb290XCIpXG4gICAgICAgIHRocm93IEVycm9yKFwiSW5pdGlhbCBwYWNrZXQgbXVzdCBiZSBmcm9tIHJvb3RcIik7XG4gICAgICBpZiAoIShcImFwcGxpY2F0aW9uVHlwZVwiIGluIHBhY2tldCkpXG4gICAgICAgIHRocm93IEVycm9yKFwiSW5pdGlhbCBwYWNrZXQgbXVzdCBjb250YWluIGFwcGxpY2F0aW9uVHlwZSBmaWVsZFwiKTtcblxuICAgICAgdGhpcy5yb290ID0gdGhpcy50eXBlU3lzdGVtLnJlYWQoXCJyb290XCIsIG51bGwsIFwicm9vdFwiKTtcbiAgICAgIHRoaXMucm9vdFxuICAgICAgICAgIC5wcm90b2NvbERlc2NyaXB0aW9uKClcbiAgICAgICAgICAuY2F0Y2gocmVjb3ZlckFjdG9yRGVzY3JpcHRpb25zKVxuICAgICAgICAgIC50aGVuKHRoaXMudHlwZVN5c3RlbS5yZWdpc3RlclR5cGVzLmJpbmQodGhpcy50eXBlU3lzdGVtKSlcbiAgICAgICAgICAudGhlbih0aGlzLm9uUmVhZHkuYmluZCh0aGlzLCB0aGlzLnJvb3QpLCB0aGlzLm9uRmFpbCk7XG4gICAgfSBlbHNlIHtcbiAgICAgIHZhciBhY3RvciA9IHRoaXMuZ2V0KHBhY2tldC5mcm9tKSB8fCB0aGlzLnJvb3Q7XG4gICAgICB2YXIgZXZlbnQgPSBhY3Rvci5ldmVudHNbcGFja2V0LnR5cGVdO1xuICAgICAgaWYgKGV2ZW50KSB7XG4gICAgICAgIGFjdG9yLmRpc3BhdGNoRXZlbnQoZXZlbnQucmVhZChwYWNrZXQpKTtcbiAgICAgIH0gZWxzZSB7XG4gICAgICAgIHZhciBpbmRleCA9IHRoaXMucmVxdWVzdHMuaW5kZXhPZihhY3Rvci5pZCk7XG4gICAgICAgIGlmIChpbmRleCA+PSAwKSB7XG4gICAgICAgICAgdmFyIHJlcXVlc3QgPSB0aGlzLnJlcXVlc3RzLnNwbGljZShpbmRleCwgMikucG9wKCk7XG4gICAgICAgICAgaWYgKHBhY2tldC5lcnJvcilcbiAgICAgICAgICAgIHJlcXVlc3QucmVqZWN0KHBhY2tldCk7XG4gICAgICAgICAgZWxzZVxuICAgICAgICAgICAgcmVxdWVzdC5yZXNvbHZlKHBhY2tldCk7XG4gICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgY29uc29sZS5lcnJvcihFcnJvcihcIlVuZXhwZWN0ZWQgcGFja2V0IFwiICsgSlNPTi5zdHJpbmdpZnkocGFja2V0LCAyLCAyKSksXG4gICAgICAgICAgICAgICAgICAgICAgICBwYWNrZXQsXG4gICAgICAgICAgICAgICAgICAgICAgICB0aGlzLnJlcXVlc3RzLnNsaWNlKDApKTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH1cbiAgfSxcblxuICBnZXQ6IGZ1bmN0aW9uKGlkKSB7XG4gICAgcmV0dXJuIHRoaXMuY2FjaGVbaWRdO1xuICB9LFxuICBzdXBlcnZpc29yT2Y6IGZ1bmN0aW9uKGFjdG9yKSB7XG4gICAgZm9yICh2YXIgaWQgaW4gdGhpcy5ncmFwaCkge1xuICAgICAgaWYgKHRoaXMuZ3JhcGhbaWRdLmluZGV4T2YoYWN0b3IuaWQpID49IDApIHtcbiAgICAgICAgcmV0dXJuIGlkO1xuICAgICAgfVxuICAgIH1cbiAgfSxcbiAgd29ya2Vyc09mOiBmdW5jdGlvbihhY3Rvcikge1xuICAgIHJldHVybiB0aGlzLmdyYXBoW2FjdG9yLmlkXTtcbiAgfSxcbiAgc3VwZXJ2aXNlOiBmdW5jdGlvbihhY3Rvciwgd29ya2VyKSB7XG4gICAgdmFyIHdvcmtlcnMgPSB0aGlzLndvcmtlcnNPZihhY3RvcilcbiAgICBpZiAod29ya2Vycy5pbmRleE9mKHdvcmtlci5pZCkgPCAwKSB7XG4gICAgICB3b3JrZXJzLnB1c2god29ya2VyLmlkKTtcbiAgICB9XG4gIH0sXG4gIHVuc3VwZXJ2aXNlOiBmdW5jdGlvbihhY3Rvciwgd29ya2VyKSB7XG4gICAgdmFyIHdvcmtlcnMgPSB0aGlzLndvcmtlcnNPZihhY3Rvcik7XG4gICAgdmFyIGluZGV4ID0gd29ya2Vycy5pbmRleE9mKHdvcmtlci5pZClcbiAgICBpZiAoaW5kZXggPj0gMCkge1xuICAgICAgd29ya2Vycy5zcGxpY2UoaW5kZXgsIDEpXG4gICAgfVxuICB9LFxuXG4gIHJlZ2lzdGVyOiBmdW5jdGlvbihhY3Rvcikge1xuICAgIHZhciByZWdpc3RlcmVkID0gdGhpcy5nZXQoYWN0b3IuaWQpO1xuICAgIGlmICghcmVnaXN0ZXJlZCkge1xuICAgICAgdGhpcy5jYWNoZVthY3Rvci5pZF0gPSBhY3RvcjtcbiAgICAgIHRoaXMuZ3JhcGhbYWN0b3IuaWRdID0gW107XG4gICAgfSBlbHNlIGlmIChyZWdpc3RlcmVkICE9PSBhY3Rvcikge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKFwiRGlmZmVyZW50IGFjdG9yIHdpdGggc2FtZSBpZCBpcyBhbHJlYWR5IHJlZ2lzdGVyZWRcIik7XG4gICAgfVxuICB9LFxuICB1bnJlZ2lzdGVyOiBmdW5jdGlvbihhY3Rvcikge1xuICAgIGlmICh0aGlzLmdldChhY3Rvci5pZCkpIHtcbiAgICAgIGRlbGV0ZSB0aGlzLmNhY2hlW2FjdG9yLmlkXTtcbiAgICAgIGRlbGV0ZSB0aGlzLmdyYXBoW2FjdG9yLmlkXTtcbiAgICB9XG4gIH0sXG5cbiAgcmVsZWFzZTogZnVuY3Rpb24oYWN0b3IpIHtcbiAgICB2YXIgc3VwZXJ2aXNvciA9IHRoaXMuc3VwZXJ2aXNvck9mKGFjdG9yKTtcbiAgICBpZiAoc3VwZXJ2aXNvcilcbiAgICAgIHRoaXMudW5zdXBlcnZpc2Uoc3VwZXJ2aXNvciwgYWN0b3IpO1xuXG4gICAgdmFyIHdvcmtlcnMgPSB0aGlzLndvcmtlcnNPZihhY3RvcilcblxuICAgIGlmICh3b3JrZXJzKSB7XG4gICAgICB3b3JrZXJzLm1hcCh0aGlzLmdldCkuZm9yRWFjaCh0aGlzLnJlbGVhc2UpXG4gICAgfVxuICAgIHRoaXMudW5lcmdpc3RlcihhY3Rvcik7XG4gIH1cbn0pO1xuZXhwb3J0cy5DbGllbnQgPSBDbGllbnQ7XG4iLCJcInVzZSBzdHJpY3RcIjtcblxudmFyIFN5bWJvbCA9IHJlcXVpcmUoXCJlczYtc3ltYm9sXCIpXG52YXIgRXZlbnRFbWl0dGVyID0gcmVxdWlyZShcImV2ZW50c1wiKS5FdmVudEVtaXR0ZXI7XG52YXIgQ2xhc3MgPSByZXF1aXJlKFwiLi9jbGFzc1wiKS5DbGFzcztcblxudmFyICRib3VuZCA9IFN5bWJvbChcIkV2ZW50VGFyZ2V0L2hhbmRsZUV2ZW50XCIpO1xudmFyICRlbWl0dGVyID0gU3ltYm9sKFwiRXZlbnRUYXJnZXQvZW1pdHRlclwiKTtcblxuZnVuY3Rpb24gbWFrZUhhbmRsZXIoaGFuZGxlcikge1xuICByZXR1cm4gZnVuY3Rpb24oZXZlbnQpIHtcbiAgICBoYW5kbGVyLmhhbmRsZUV2ZW50KGV2ZW50KTtcbiAgfVxufVxuXG52YXIgRXZlbnRUYXJnZXQgPSBDbGFzcyh7XG4gIGNvbnN0cnVjdG9yOiBmdW5jdGlvbigpIHtcbiAgICBPYmplY3QuZGVmaW5lUHJvcGVydHkodGhpcywgJGVtaXR0ZXIsIHtcbiAgICAgIGVudW1lcmFibGU6IGZhbHNlLFxuICAgICAgY29uZmlndXJhYmxlOiB0cnVlLFxuICAgICAgd3JpdGFibGU6IHRydWUsXG4gICAgICB2YWx1ZTogbmV3IEV2ZW50RW1pdHRlcigpXG4gICAgfSk7XG4gIH0sXG4gIGFkZEV2ZW50TGlzdGVuZXI6IGZ1bmN0aW9uKHR5cGUsIGhhbmRsZXIpIHtcbiAgICBpZiAodHlwZW9mKGhhbmRsZXIpID09PSBcImZ1bmN0aW9uXCIpIHtcbiAgICAgIHRoaXNbJGVtaXR0ZXJdLm9uKHR5cGUsIGhhbmRsZXIpO1xuICAgIH1cbiAgICBlbHNlIGlmIChoYW5kbGVyICYmIHR5cGVvZihoYW5kbGVyKSA9PT0gXCJvYmplY3RcIikge1xuICAgICAgaWYgKCFoYW5kbGVyWyRib3VuZF0pIGhhbmRsZXJbJGJvdW5kXSA9IG1ha2VIYW5kbGVyKGhhbmRsZXIpO1xuICAgICAgdGhpc1skZW1pdHRlcl0ub24odHlwZSwgaGFuZGxlclskYm91bmRdKTtcbiAgICB9XG4gIH0sXG4gIHJlbW92ZUV2ZW50TGlzdGVuZXI6IGZ1bmN0aW9uKHR5cGUsIGhhbmRsZXIpIHtcbiAgICBpZiAodHlwZW9mKGhhbmRsZXIpID09PSBcImZ1bmN0aW9uXCIpXG4gICAgICB0aGlzWyRlbWl0dGVyXS5yZW1vdmVMaXN0ZW5lcih0eXBlLCBoYW5kbGVyKTtcbiAgICBlbHNlIGlmIChoYW5kbGVyICYmIGhhbmRsZXJbJGJvdW5kXSlcbiAgICAgIHRoaXNbJGVtaXR0ZXJdLnJlbW92ZUxpc3RlbmVyKHR5cGUsIGhhbmRsZXJbJGJvdW5kXSk7XG4gIH0sXG4gIGRpc3BhdGNoRXZlbnQ6IGZ1bmN0aW9uKGV2ZW50KSB7XG4gICAgZXZlbnQudGFyZ2V0ID0gdGhpcztcbiAgICB0aGlzWyRlbWl0dGVyXS5lbWl0KGV2ZW50LnR5cGUsIGV2ZW50KTtcbiAgfVxufSk7XG5leHBvcnRzLkV2ZW50VGFyZ2V0ID0gRXZlbnRUYXJnZXQ7XG5cbnZhciBNZXNzYWdlRXZlbnQgPSBDbGFzcyh7XG4gIGNvbnN0cnVjdG9yOiBmdW5jdGlvbih0eXBlLCBvcHRpb25zKSB7XG4gICAgb3B0aW9ucyA9IG9wdGlvbnMgfHwge307XG4gICAgdGhpcy50eXBlID0gdHlwZTtcbiAgICB0aGlzLmRhdGEgPSBvcHRpb25zLmRhdGEgPT09IHZvaWQoMCkgPyBudWxsIDogb3B0aW9ucy5kYXRhO1xuXG4gICAgdGhpcy5sYXN0RXZlbnRJZCA9IG9wdGlvbnMubGFzdEV2ZW50SWQgfHwgXCJcIjtcbiAgICB0aGlzLm9yaWdpbiA9IG9wdGlvbnMub3JpZ2luIHx8IFwiXCI7XG4gICAgdGhpcy5idWJibGVzID0gb3B0aW9ucy5idWJibGVzIHx8IGZhbHNlO1xuICAgIHRoaXMuY2FuY2VsYWJsZSA9IG9wdGlvbnMuY2FuY2VsYWJsZSB8fCBmYWxzZTtcbiAgfSxcbiAgc291cmNlOiBudWxsLFxuICBwb3J0czogbnVsbCxcbiAgcHJldmVudERlZmF1bHQ6IGZ1bmN0aW9uKCkge1xuICB9LFxuICBzdG9wUHJvcGFnYXRpb246IGZ1bmN0aW9uKCkge1xuICB9LFxuICBzdG9wSW1tZWRpYXRlUHJvcGFnYXRpb246IGZ1bmN0aW9uKCkge1xuICB9XG59KTtcbmV4cG9ydHMuTWVzc2FnZUV2ZW50ID0gTWVzc2FnZUV2ZW50O1xuIiwiLy8gQ29weXJpZ2h0IEpveWVudCwgSW5jLiBhbmQgb3RoZXIgTm9kZSBjb250cmlidXRvcnMuXG4vL1xuLy8gUGVybWlzc2lvbiBpcyBoZXJlYnkgZ3JhbnRlZCwgZnJlZSBvZiBjaGFyZ2UsIHRvIGFueSBwZXJzb24gb2J0YWluaW5nIGFcbi8vIGNvcHkgb2YgdGhpcyBzb2Z0d2FyZSBhbmQgYXNzb2NpYXRlZCBkb2N1bWVudGF0aW9uIGZpbGVzICh0aGVcbi8vIFwiU29mdHdhcmVcIiksIHRvIGRlYWwgaW4gdGhlIFNvZnR3YXJlIHdpdGhvdXQgcmVzdHJpY3Rpb24sIGluY2x1ZGluZ1xuLy8gd2l0aG91dCBsaW1pdGF0aW9uIHRoZSByaWdodHMgdG8gdXNlLCBjb3B5LCBtb2RpZnksIG1lcmdlLCBwdWJsaXNoLFxuLy8gZGlzdHJpYnV0ZSwgc3VibGljZW5zZSwgYW5kL29yIHNlbGwgY29waWVzIG9mIHRoZSBTb2Z0d2FyZSwgYW5kIHRvIHBlcm1pdFxuLy8gcGVyc29ucyB0byB3aG9tIHRoZSBTb2Z0d2FyZSBpcyBmdXJuaXNoZWQgdG8gZG8gc28sIHN1YmplY3QgdG8gdGhlXG4vLyBmb2xsb3dpbmcgY29uZGl0aW9uczpcbi8vXG4vLyBUaGUgYWJvdmUgY29weXJpZ2h0IG5vdGljZSBhbmQgdGhpcyBwZXJtaXNzaW9uIG5vdGljZSBzaGFsbCBiZSBpbmNsdWRlZFxuLy8gaW4gYWxsIGNvcGllcyBvciBzdWJzdGFudGlhbCBwb3J0aW9ucyBvZiB0aGUgU29mdHdhcmUuXG4vL1xuLy8gVEhFIFNPRlRXQVJFIElTIFBST1ZJREVEIFwiQVMgSVNcIiwgV0lUSE9VVCBXQVJSQU5UWSBPRiBBTlkgS0lORCwgRVhQUkVTU1xuLy8gT1IgSU1QTElFRCwgSU5DTFVESU5HIEJVVCBOT1QgTElNSVRFRCBUTyBUSEUgV0FSUkFOVElFUyBPRlxuLy8gTUVSQ0hBTlRBQklMSVRZLCBGSVRORVNTIEZPUiBBIFBBUlRJQ1VMQVIgUFVSUE9TRSBBTkQgTk9OSU5GUklOR0VNRU5ULiBJTlxuLy8gTk8gRVZFTlQgU0hBTEwgVEhFIEFVVEhPUlMgT1IgQ09QWVJJR0hUIEhPTERFUlMgQkUgTElBQkxFIEZPUiBBTlkgQ0xBSU0sXG4vLyBEQU1BR0VTIE9SIE9USEVSIExJQUJJTElUWSwgV0hFVEhFUiBJTiBBTiBBQ1RJT04gT0YgQ09OVFJBQ1QsIFRPUlQgT1Jcbi8vIE9USEVSV0lTRSwgQVJJU0lORyBGUk9NLCBPVVQgT0YgT1IgSU4gQ09OTkVDVElPTiBXSVRIIFRIRSBTT0ZUV0FSRSBPUiBUSEVcbi8vIFVTRSBPUiBPVEhFUiBERUFMSU5HUyBJTiBUSEUgU09GVFdBUkUuXG5cbmZ1bmN0aW9uIEV2ZW50RW1pdHRlcigpIHtcbiAgdGhpcy5fZXZlbnRzID0gdGhpcy5fZXZlbnRzIHx8IHt9O1xuICB0aGlzLl9tYXhMaXN0ZW5lcnMgPSB0aGlzLl9tYXhMaXN0ZW5lcnMgfHwgdW5kZWZpbmVkO1xufVxubW9kdWxlLmV4cG9ydHMgPSBFdmVudEVtaXR0ZXI7XG5cbi8vIEJhY2t3YXJkcy1jb21wYXQgd2l0aCBub2RlIDAuMTAueFxuRXZlbnRFbWl0dGVyLkV2ZW50RW1pdHRlciA9IEV2ZW50RW1pdHRlcjtcblxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5fZXZlbnRzID0gdW5kZWZpbmVkO1xuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5fbWF4TGlzdGVuZXJzID0gdW5kZWZpbmVkO1xuXG4vLyBCeSBkZWZhdWx0IEV2ZW50RW1pdHRlcnMgd2lsbCBwcmludCBhIHdhcm5pbmcgaWYgbW9yZSB0aGFuIDEwIGxpc3RlbmVycyBhcmVcbi8vIGFkZGVkIHRvIGl0LiBUaGlzIGlzIGEgdXNlZnVsIGRlZmF1bHQgd2hpY2ggaGVscHMgZmluZGluZyBtZW1vcnkgbGVha3MuXG5FdmVudEVtaXR0ZXIuZGVmYXVsdE1heExpc3RlbmVycyA9IDEwO1xuXG4vLyBPYnZpb3VzbHkgbm90IGFsbCBFbWl0dGVycyBzaG91bGQgYmUgbGltaXRlZCB0byAxMC4gVGhpcyBmdW5jdGlvbiBhbGxvd3Ncbi8vIHRoYXQgdG8gYmUgaW5jcmVhc2VkLiBTZXQgdG8gemVybyBmb3IgdW5saW1pdGVkLlxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5zZXRNYXhMaXN0ZW5lcnMgPSBmdW5jdGlvbihuKSB7XG4gIGlmICghaXNOdW1iZXIobikgfHwgbiA8IDAgfHwgaXNOYU4obikpXG4gICAgdGhyb3cgVHlwZUVycm9yKCduIG11c3QgYmUgYSBwb3NpdGl2ZSBudW1iZXInKTtcbiAgdGhpcy5fbWF4TGlzdGVuZXJzID0gbjtcbiAgcmV0dXJuIHRoaXM7XG59O1xuXG5FdmVudEVtaXR0ZXIucHJvdG90eXBlLmVtaXQgPSBmdW5jdGlvbih0eXBlKSB7XG4gIHZhciBlciwgaGFuZGxlciwgbGVuLCBhcmdzLCBpLCBsaXN0ZW5lcnM7XG5cbiAgaWYgKCF0aGlzLl9ldmVudHMpXG4gICAgdGhpcy5fZXZlbnRzID0ge307XG5cbiAgLy8gSWYgdGhlcmUgaXMgbm8gJ2Vycm9yJyBldmVudCBsaXN0ZW5lciB0aGVuIHRocm93LlxuICBpZiAodHlwZSA9PT0gJ2Vycm9yJykge1xuICAgIGlmICghdGhpcy5fZXZlbnRzLmVycm9yIHx8XG4gICAgICAgIChpc09iamVjdCh0aGlzLl9ldmVudHMuZXJyb3IpICYmICF0aGlzLl9ldmVudHMuZXJyb3IubGVuZ3RoKSkge1xuICAgICAgZXIgPSBhcmd1bWVudHNbMV07XG4gICAgICBpZiAoZXIgaW5zdGFuY2VvZiBFcnJvcikge1xuICAgICAgICB0aHJvdyBlcjsgLy8gVW5oYW5kbGVkICdlcnJvcicgZXZlbnRcbiAgICAgIH0gZWxzZSB7XG4gICAgICAgIHRocm93IFR5cGVFcnJvcignVW5jYXVnaHQsIHVuc3BlY2lmaWVkIFwiZXJyb3JcIiBldmVudC4nKTtcbiAgICAgIH1cbiAgICAgIHJldHVybiBmYWxzZTtcbiAgICB9XG4gIH1cblxuICBoYW5kbGVyID0gdGhpcy5fZXZlbnRzW3R5cGVdO1xuXG4gIGlmIChpc1VuZGVmaW5lZChoYW5kbGVyKSlcbiAgICByZXR1cm4gZmFsc2U7XG5cbiAgaWYgKGlzRnVuY3Rpb24oaGFuZGxlcikpIHtcbiAgICBzd2l0Y2ggKGFyZ3VtZW50cy5sZW5ndGgpIHtcbiAgICAgIC8vIGZhc3QgY2FzZXNcbiAgICAgIGNhc2UgMTpcbiAgICAgICAgaGFuZGxlci5jYWxsKHRoaXMpO1xuICAgICAgICBicmVhaztcbiAgICAgIGNhc2UgMjpcbiAgICAgICAgaGFuZGxlci5jYWxsKHRoaXMsIGFyZ3VtZW50c1sxXSk7XG4gICAgICAgIGJyZWFrO1xuICAgICAgY2FzZSAzOlxuICAgICAgICBoYW5kbGVyLmNhbGwodGhpcywgYXJndW1lbnRzWzFdLCBhcmd1bWVudHNbMl0pO1xuICAgICAgICBicmVhaztcbiAgICAgIC8vIHNsb3dlclxuICAgICAgZGVmYXVsdDpcbiAgICAgICAgbGVuID0gYXJndW1lbnRzLmxlbmd0aDtcbiAgICAgICAgYXJncyA9IG5ldyBBcnJheShsZW4gLSAxKTtcbiAgICAgICAgZm9yIChpID0gMTsgaSA8IGxlbjsgaSsrKVxuICAgICAgICAgIGFyZ3NbaSAtIDFdID0gYXJndW1lbnRzW2ldO1xuICAgICAgICBoYW5kbGVyLmFwcGx5KHRoaXMsIGFyZ3MpO1xuICAgIH1cbiAgfSBlbHNlIGlmIChpc09iamVjdChoYW5kbGVyKSkge1xuICAgIGxlbiA9IGFyZ3VtZW50cy5sZW5ndGg7XG4gICAgYXJncyA9IG5ldyBBcnJheShsZW4gLSAxKTtcbiAgICBmb3IgKGkgPSAxOyBpIDwgbGVuOyBpKyspXG4gICAgICBhcmdzW2kgLSAxXSA9IGFyZ3VtZW50c1tpXTtcblxuICAgIGxpc3RlbmVycyA9IGhhbmRsZXIuc2xpY2UoKTtcbiAgICBsZW4gPSBsaXN0ZW5lcnMubGVuZ3RoO1xuICAgIGZvciAoaSA9IDA7IGkgPCBsZW47IGkrKylcbiAgICAgIGxpc3RlbmVyc1tpXS5hcHBseSh0aGlzLCBhcmdzKTtcbiAgfVxuXG4gIHJldHVybiB0cnVlO1xufTtcblxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5hZGRMaXN0ZW5lciA9IGZ1bmN0aW9uKHR5cGUsIGxpc3RlbmVyKSB7XG4gIHZhciBtO1xuXG4gIGlmICghaXNGdW5jdGlvbihsaXN0ZW5lcikpXG4gICAgdGhyb3cgVHlwZUVycm9yKCdsaXN0ZW5lciBtdXN0IGJlIGEgZnVuY3Rpb24nKTtcblxuICBpZiAoIXRoaXMuX2V2ZW50cylcbiAgICB0aGlzLl9ldmVudHMgPSB7fTtcblxuICAvLyBUbyBhdm9pZCByZWN1cnNpb24gaW4gdGhlIGNhc2UgdGhhdCB0eXBlID09PSBcIm5ld0xpc3RlbmVyXCIhIEJlZm9yZVxuICAvLyBhZGRpbmcgaXQgdG8gdGhlIGxpc3RlbmVycywgZmlyc3QgZW1pdCBcIm5ld0xpc3RlbmVyXCIuXG4gIGlmICh0aGlzLl9ldmVudHMubmV3TGlzdGVuZXIpXG4gICAgdGhpcy5lbWl0KCduZXdMaXN0ZW5lcicsIHR5cGUsXG4gICAgICAgICAgICAgIGlzRnVuY3Rpb24obGlzdGVuZXIubGlzdGVuZXIpID9cbiAgICAgICAgICAgICAgbGlzdGVuZXIubGlzdGVuZXIgOiBsaXN0ZW5lcik7XG5cbiAgaWYgKCF0aGlzLl9ldmVudHNbdHlwZV0pXG4gICAgLy8gT3B0aW1pemUgdGhlIGNhc2Ugb2Ygb25lIGxpc3RlbmVyLiBEb24ndCBuZWVkIHRoZSBleHRyYSBhcnJheSBvYmplY3QuXG4gICAgdGhpcy5fZXZlbnRzW3R5cGVdID0gbGlzdGVuZXI7XG4gIGVsc2UgaWYgKGlzT2JqZWN0KHRoaXMuX2V2ZW50c1t0eXBlXSkpXG4gICAgLy8gSWYgd2UndmUgYWxyZWFkeSBnb3QgYW4gYXJyYXksIGp1c3QgYXBwZW5kLlxuICAgIHRoaXMuX2V2ZW50c1t0eXBlXS5wdXNoKGxpc3RlbmVyKTtcbiAgZWxzZVxuICAgIC8vIEFkZGluZyB0aGUgc2Vjb25kIGVsZW1lbnQsIG5lZWQgdG8gY2hhbmdlIHRvIGFycmF5LlxuICAgIHRoaXMuX2V2ZW50c1t0eXBlXSA9IFt0aGlzLl9ldmVudHNbdHlwZV0sIGxpc3RlbmVyXTtcblxuICAvLyBDaGVjayBmb3IgbGlzdGVuZXIgbGVha1xuICBpZiAoaXNPYmplY3QodGhpcy5fZXZlbnRzW3R5cGVdKSAmJiAhdGhpcy5fZXZlbnRzW3R5cGVdLndhcm5lZCkge1xuICAgIHZhciBtO1xuICAgIGlmICghaXNVbmRlZmluZWQodGhpcy5fbWF4TGlzdGVuZXJzKSkge1xuICAgICAgbSA9IHRoaXMuX21heExpc3RlbmVycztcbiAgICB9IGVsc2Uge1xuICAgICAgbSA9IEV2ZW50RW1pdHRlci5kZWZhdWx0TWF4TGlzdGVuZXJzO1xuICAgIH1cblxuICAgIGlmIChtICYmIG0gPiAwICYmIHRoaXMuX2V2ZW50c1t0eXBlXS5sZW5ndGggPiBtKSB7XG4gICAgICB0aGlzLl9ldmVudHNbdHlwZV0ud2FybmVkID0gdHJ1ZTtcbiAgICAgIGNvbnNvbGUuZXJyb3IoJyhub2RlKSB3YXJuaW5nOiBwb3NzaWJsZSBFdmVudEVtaXR0ZXIgbWVtb3J5ICcgK1xuICAgICAgICAgICAgICAgICAgICAnbGVhayBkZXRlY3RlZC4gJWQgbGlzdGVuZXJzIGFkZGVkLiAnICtcbiAgICAgICAgICAgICAgICAgICAgJ1VzZSBlbWl0dGVyLnNldE1heExpc3RlbmVycygpIHRvIGluY3JlYXNlIGxpbWl0LicsXG4gICAgICAgICAgICAgICAgICAgIHRoaXMuX2V2ZW50c1t0eXBlXS5sZW5ndGgpO1xuICAgICAgY29uc29sZS50cmFjZSgpO1xuICAgIH1cbiAgfVxuXG4gIHJldHVybiB0aGlzO1xufTtcblxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5vbiA9IEV2ZW50RW1pdHRlci5wcm90b3R5cGUuYWRkTGlzdGVuZXI7XG5cbkV2ZW50RW1pdHRlci5wcm90b3R5cGUub25jZSA9IGZ1bmN0aW9uKHR5cGUsIGxpc3RlbmVyKSB7XG4gIGlmICghaXNGdW5jdGlvbihsaXN0ZW5lcikpXG4gICAgdGhyb3cgVHlwZUVycm9yKCdsaXN0ZW5lciBtdXN0IGJlIGEgZnVuY3Rpb24nKTtcblxuICB2YXIgZmlyZWQgPSBmYWxzZTtcblxuICBmdW5jdGlvbiBnKCkge1xuICAgIHRoaXMucmVtb3ZlTGlzdGVuZXIodHlwZSwgZyk7XG5cbiAgICBpZiAoIWZpcmVkKSB7XG4gICAgICBmaXJlZCA9IHRydWU7XG4gICAgICBsaXN0ZW5lci5hcHBseSh0aGlzLCBhcmd1bWVudHMpO1xuICAgIH1cbiAgfVxuXG4gIGcubGlzdGVuZXIgPSBsaXN0ZW5lcjtcbiAgdGhpcy5vbih0eXBlLCBnKTtcblxuICByZXR1cm4gdGhpcztcbn07XG5cbi8vIGVtaXRzIGEgJ3JlbW92ZUxpc3RlbmVyJyBldmVudCBpZmYgdGhlIGxpc3RlbmVyIHdhcyByZW1vdmVkXG5FdmVudEVtaXR0ZXIucHJvdG90eXBlLnJlbW92ZUxpc3RlbmVyID0gZnVuY3Rpb24odHlwZSwgbGlzdGVuZXIpIHtcbiAgdmFyIGxpc3QsIHBvc2l0aW9uLCBsZW5ndGgsIGk7XG5cbiAgaWYgKCFpc0Z1bmN0aW9uKGxpc3RlbmVyKSlcbiAgICB0aHJvdyBUeXBlRXJyb3IoJ2xpc3RlbmVyIG11c3QgYmUgYSBmdW5jdGlvbicpO1xuXG4gIGlmICghdGhpcy5fZXZlbnRzIHx8ICF0aGlzLl9ldmVudHNbdHlwZV0pXG4gICAgcmV0dXJuIHRoaXM7XG5cbiAgbGlzdCA9IHRoaXMuX2V2ZW50c1t0eXBlXTtcbiAgbGVuZ3RoID0gbGlzdC5sZW5ndGg7XG4gIHBvc2l0aW9uID0gLTE7XG5cbiAgaWYgKGxpc3QgPT09IGxpc3RlbmVyIHx8XG4gICAgICAoaXNGdW5jdGlvbihsaXN0Lmxpc3RlbmVyKSAmJiBsaXN0Lmxpc3RlbmVyID09PSBsaXN0ZW5lcikpIHtcbiAgICBkZWxldGUgdGhpcy5fZXZlbnRzW3R5cGVdO1xuICAgIGlmICh0aGlzLl9ldmVudHMucmVtb3ZlTGlzdGVuZXIpXG4gICAgICB0aGlzLmVtaXQoJ3JlbW92ZUxpc3RlbmVyJywgdHlwZSwgbGlzdGVuZXIpO1xuXG4gIH0gZWxzZSBpZiAoaXNPYmplY3QobGlzdCkpIHtcbiAgICBmb3IgKGkgPSBsZW5ndGg7IGktLSA+IDA7KSB7XG4gICAgICBpZiAobGlzdFtpXSA9PT0gbGlzdGVuZXIgfHxcbiAgICAgICAgICAobGlzdFtpXS5saXN0ZW5lciAmJiBsaXN0W2ldLmxpc3RlbmVyID09PSBsaXN0ZW5lcikpIHtcbiAgICAgICAgcG9zaXRpb24gPSBpO1xuICAgICAgICBicmVhaztcbiAgICAgIH1cbiAgICB9XG5cbiAgICBpZiAocG9zaXRpb24gPCAwKVxuICAgICAgcmV0dXJuIHRoaXM7XG5cbiAgICBpZiAobGlzdC5sZW5ndGggPT09IDEpIHtcbiAgICAgIGxpc3QubGVuZ3RoID0gMDtcbiAgICAgIGRlbGV0ZSB0aGlzLl9ldmVudHNbdHlwZV07XG4gICAgfSBlbHNlIHtcbiAgICAgIGxpc3Quc3BsaWNlKHBvc2l0aW9uLCAxKTtcbiAgICB9XG5cbiAgICBpZiAodGhpcy5fZXZlbnRzLnJlbW92ZUxpc3RlbmVyKVxuICAgICAgdGhpcy5lbWl0KCdyZW1vdmVMaXN0ZW5lcicsIHR5cGUsIGxpc3RlbmVyKTtcbiAgfVxuXG4gIHJldHVybiB0aGlzO1xufTtcblxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5yZW1vdmVBbGxMaXN0ZW5lcnMgPSBmdW5jdGlvbih0eXBlKSB7XG4gIHZhciBrZXksIGxpc3RlbmVycztcblxuICBpZiAoIXRoaXMuX2V2ZW50cylcbiAgICByZXR1cm4gdGhpcztcblxuICAvLyBub3QgbGlzdGVuaW5nIGZvciByZW1vdmVMaXN0ZW5lciwgbm8gbmVlZCB0byBlbWl0XG4gIGlmICghdGhpcy5fZXZlbnRzLnJlbW92ZUxpc3RlbmVyKSB7XG4gICAgaWYgKGFyZ3VtZW50cy5sZW5ndGggPT09IDApXG4gICAgICB0aGlzLl9ldmVudHMgPSB7fTtcbiAgICBlbHNlIGlmICh0aGlzLl9ldmVudHNbdHlwZV0pXG4gICAgICBkZWxldGUgdGhpcy5fZXZlbnRzW3R5cGVdO1xuICAgIHJldHVybiB0aGlzO1xuICB9XG5cbiAgLy8gZW1pdCByZW1vdmVMaXN0ZW5lciBmb3IgYWxsIGxpc3RlbmVycyBvbiBhbGwgZXZlbnRzXG4gIGlmIChhcmd1bWVudHMubGVuZ3RoID09PSAwKSB7XG4gICAgZm9yIChrZXkgaW4gdGhpcy5fZXZlbnRzKSB7XG4gICAgICBpZiAoa2V5ID09PSAncmVtb3ZlTGlzdGVuZXInKSBjb250aW51ZTtcbiAgICAgIHRoaXMucmVtb3ZlQWxsTGlzdGVuZXJzKGtleSk7XG4gICAgfVxuICAgIHRoaXMucmVtb3ZlQWxsTGlzdGVuZXJzKCdyZW1vdmVMaXN0ZW5lcicpO1xuICAgIHRoaXMuX2V2ZW50cyA9IHt9O1xuICAgIHJldHVybiB0aGlzO1xuICB9XG5cbiAgbGlzdGVuZXJzID0gdGhpcy5fZXZlbnRzW3R5cGVdO1xuXG4gIGlmIChpc0Z1bmN0aW9uKGxpc3RlbmVycykpIHtcbiAgICB0aGlzLnJlbW92ZUxpc3RlbmVyKHR5cGUsIGxpc3RlbmVycyk7XG4gIH0gZWxzZSB7XG4gICAgLy8gTElGTyBvcmRlclxuICAgIHdoaWxlIChsaXN0ZW5lcnMubGVuZ3RoKVxuICAgICAgdGhpcy5yZW1vdmVMaXN0ZW5lcih0eXBlLCBsaXN0ZW5lcnNbbGlzdGVuZXJzLmxlbmd0aCAtIDFdKTtcbiAgfVxuICBkZWxldGUgdGhpcy5fZXZlbnRzW3R5cGVdO1xuXG4gIHJldHVybiB0aGlzO1xufTtcblxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5saXN0ZW5lcnMgPSBmdW5jdGlvbih0eXBlKSB7XG4gIHZhciByZXQ7XG4gIGlmICghdGhpcy5fZXZlbnRzIHx8ICF0aGlzLl9ldmVudHNbdHlwZV0pXG4gICAgcmV0ID0gW107XG4gIGVsc2UgaWYgKGlzRnVuY3Rpb24odGhpcy5fZXZlbnRzW3R5cGVdKSlcbiAgICByZXQgPSBbdGhpcy5fZXZlbnRzW3R5cGVdXTtcbiAgZWxzZVxuICAgIHJldCA9IHRoaXMuX2V2ZW50c1t0eXBlXS5zbGljZSgpO1xuICByZXR1cm4gcmV0O1xufTtcblxuRXZlbnRFbWl0dGVyLmxpc3RlbmVyQ291bnQgPSBmdW5jdGlvbihlbWl0dGVyLCB0eXBlKSB7XG4gIHZhciByZXQ7XG4gIGlmICghZW1pdHRlci5fZXZlbnRzIHx8ICFlbWl0dGVyLl9ldmVudHNbdHlwZV0pXG4gICAgcmV0ID0gMDtcbiAgZWxzZSBpZiAoaXNGdW5jdGlvbihlbWl0dGVyLl9ldmVudHNbdHlwZV0pKVxuICAgIHJldCA9IDE7XG4gIGVsc2VcbiAgICByZXQgPSBlbWl0dGVyLl9ldmVudHNbdHlwZV0ubGVuZ3RoO1xuICByZXR1cm4gcmV0O1xufTtcblxuZnVuY3Rpb24gaXNGdW5jdGlvbihhcmcpIHtcbiAgcmV0dXJuIHR5cGVvZiBhcmcgPT09ICdmdW5jdGlvbic7XG59XG5cbmZ1bmN0aW9uIGlzTnVtYmVyKGFyZykge1xuICByZXR1cm4gdHlwZW9mIGFyZyA9PT0gJ251bWJlcic7XG59XG5cbmZ1bmN0aW9uIGlzT2JqZWN0KGFyZykge1xuICByZXR1cm4gdHlwZW9mIGFyZyA9PT0gJ29iamVjdCcgJiYgYXJnICE9PSBudWxsO1xufVxuXG5mdW5jdGlvbiBpc1VuZGVmaW5lZChhcmcpIHtcbiAgcmV0dXJuIGFyZyA9PT0gdm9pZCAwO1xufVxuIiwiJ3VzZSBzdHJpY3QnO1xuXG5tb2R1bGUuZXhwb3J0cyA9IHJlcXVpcmUoJy4vaXMtaW1wbGVtZW50ZWQnKSgpID8gU3ltYm9sIDogcmVxdWlyZSgnLi9wb2x5ZmlsbCcpO1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG5tb2R1bGUuZXhwb3J0cyA9IGZ1bmN0aW9uICgpIHtcblx0dmFyIHN5bWJvbDtcblx0aWYgKHR5cGVvZiBTeW1ib2wgIT09ICdmdW5jdGlvbicpIHJldHVybiBmYWxzZTtcblx0c3ltYm9sID0gU3ltYm9sKCd0ZXN0IHN5bWJvbCcpO1xuXHR0cnkge1xuXHRcdGlmIChTdHJpbmcoc3ltYm9sKSAhPT0gJ1N5bWJvbCAodGVzdCBzeW1ib2wpJykgcmV0dXJuIGZhbHNlO1xuXHR9IGNhdGNoIChlKSB7IHJldHVybiBmYWxzZTsgfVxuXHRpZiAodHlwZW9mIFN5bWJvbC5pdGVyYXRvciA9PT0gJ3N5bWJvbCcpIHJldHVybiB0cnVlO1xuXG5cdC8vIFJldHVybiAndHJ1ZScgZm9yIHBvbHlmaWxsc1xuXHRpZiAodHlwZW9mIFN5bWJvbC5pc0NvbmNhdFNwcmVhZGFibGUgIT09ICdvYmplY3QnKSByZXR1cm4gZmFsc2U7XG5cdGlmICh0eXBlb2YgU3ltYm9sLmlzUmVnRXhwICE9PSAnb2JqZWN0JykgcmV0dXJuIGZhbHNlO1xuXHRpZiAodHlwZW9mIFN5bWJvbC5pdGVyYXRvciAhPT0gJ29iamVjdCcpIHJldHVybiBmYWxzZTtcblx0aWYgKHR5cGVvZiBTeW1ib2wudG9QcmltaXRpdmUgIT09ICdvYmplY3QnKSByZXR1cm4gZmFsc2U7XG5cdGlmICh0eXBlb2YgU3ltYm9sLnRvU3RyaW5nVGFnICE9PSAnb2JqZWN0JykgcmV0dXJuIGZhbHNlO1xuXHRpZiAodHlwZW9mIFN5bWJvbC51bnNjb3BhYmxlcyAhPT0gJ29iamVjdCcpIHJldHVybiBmYWxzZTtcblxuXHRyZXR1cm4gdHJ1ZTtcbn07XG4iLCIndXNlIHN0cmljdCc7XG5cbnZhciBhc3NpZ24gICAgICAgID0gcmVxdWlyZSgnZXM1LWV4dC9vYmplY3QvYXNzaWduJylcbiAgLCBub3JtYWxpemVPcHRzID0gcmVxdWlyZSgnZXM1LWV4dC9vYmplY3Qvbm9ybWFsaXplLW9wdGlvbnMnKVxuICAsIGlzQ2FsbGFibGUgICAgPSByZXF1aXJlKCdlczUtZXh0L29iamVjdC9pcy1jYWxsYWJsZScpXG4gICwgY29udGFpbnMgICAgICA9IHJlcXVpcmUoJ2VzNS1leHQvc3RyaW5nLyMvY29udGFpbnMnKVxuXG4gICwgZDtcblxuZCA9IG1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKGRzY3IsIHZhbHVlLyosIG9wdGlvbnMqLykge1xuXHR2YXIgYywgZSwgdywgb3B0aW9ucywgZGVzYztcblx0aWYgKChhcmd1bWVudHMubGVuZ3RoIDwgMikgfHwgKHR5cGVvZiBkc2NyICE9PSAnc3RyaW5nJykpIHtcblx0XHRvcHRpb25zID0gdmFsdWU7XG5cdFx0dmFsdWUgPSBkc2NyO1xuXHRcdGRzY3IgPSBudWxsO1xuXHR9IGVsc2Uge1xuXHRcdG9wdGlvbnMgPSBhcmd1bWVudHNbMl07XG5cdH1cblx0aWYgKGRzY3IgPT0gbnVsbCkge1xuXHRcdGMgPSB3ID0gdHJ1ZTtcblx0XHRlID0gZmFsc2U7XG5cdH0gZWxzZSB7XG5cdFx0YyA9IGNvbnRhaW5zLmNhbGwoZHNjciwgJ2MnKTtcblx0XHRlID0gY29udGFpbnMuY2FsbChkc2NyLCAnZScpO1xuXHRcdHcgPSBjb250YWlucy5jYWxsKGRzY3IsICd3Jyk7XG5cdH1cblxuXHRkZXNjID0geyB2YWx1ZTogdmFsdWUsIGNvbmZpZ3VyYWJsZTogYywgZW51bWVyYWJsZTogZSwgd3JpdGFibGU6IHcgfTtcblx0cmV0dXJuICFvcHRpb25zID8gZGVzYyA6IGFzc2lnbihub3JtYWxpemVPcHRzKG9wdGlvbnMpLCBkZXNjKTtcbn07XG5cbmQuZ3MgPSBmdW5jdGlvbiAoZHNjciwgZ2V0LCBzZXQvKiwgb3B0aW9ucyovKSB7XG5cdHZhciBjLCBlLCBvcHRpb25zLCBkZXNjO1xuXHRpZiAodHlwZW9mIGRzY3IgIT09ICdzdHJpbmcnKSB7XG5cdFx0b3B0aW9ucyA9IHNldDtcblx0XHRzZXQgPSBnZXQ7XG5cdFx0Z2V0ID0gZHNjcjtcblx0XHRkc2NyID0gbnVsbDtcblx0fSBlbHNlIHtcblx0XHRvcHRpb25zID0gYXJndW1lbnRzWzNdO1xuXHR9XG5cdGlmIChnZXQgPT0gbnVsbCkge1xuXHRcdGdldCA9IHVuZGVmaW5lZDtcblx0fSBlbHNlIGlmICghaXNDYWxsYWJsZShnZXQpKSB7XG5cdFx0b3B0aW9ucyA9IGdldDtcblx0XHRnZXQgPSBzZXQgPSB1bmRlZmluZWQ7XG5cdH0gZWxzZSBpZiAoc2V0ID09IG51bGwpIHtcblx0XHRzZXQgPSB1bmRlZmluZWQ7XG5cdH0gZWxzZSBpZiAoIWlzQ2FsbGFibGUoc2V0KSkge1xuXHRcdG9wdGlvbnMgPSBzZXQ7XG5cdFx0c2V0ID0gdW5kZWZpbmVkO1xuXHR9XG5cdGlmIChkc2NyID09IG51bGwpIHtcblx0XHRjID0gdHJ1ZTtcblx0XHRlID0gZmFsc2U7XG5cdH0gZWxzZSB7XG5cdFx0YyA9IGNvbnRhaW5zLmNhbGwoZHNjciwgJ2MnKTtcblx0XHRlID0gY29udGFpbnMuY2FsbChkc2NyLCAnZScpO1xuXHR9XG5cblx0ZGVzYyA9IHsgZ2V0OiBnZXQsIHNldDogc2V0LCBjb25maWd1cmFibGU6IGMsIGVudW1lcmFibGU6IGUgfTtcblx0cmV0dXJuICFvcHRpb25zID8gZGVzYyA6IGFzc2lnbihub3JtYWxpemVPcHRzKG9wdGlvbnMpLCBkZXNjKTtcbn07XG4iLCIndXNlIHN0cmljdCc7XG5cbm1vZHVsZS5leHBvcnRzID0gcmVxdWlyZSgnLi9pcy1pbXBsZW1lbnRlZCcpKClcblx0PyBPYmplY3QuYXNzaWduXG5cdDogcmVxdWlyZSgnLi9zaGltJyk7XG4iLCIndXNlIHN0cmljdCc7XG5cbm1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKCkge1xuXHR2YXIgYXNzaWduID0gT2JqZWN0LmFzc2lnbiwgb2JqO1xuXHRpZiAodHlwZW9mIGFzc2lnbiAhPT0gJ2Z1bmN0aW9uJykgcmV0dXJuIGZhbHNlO1xuXHRvYmogPSB7IGZvbzogJ3JheicgfTtcblx0YXNzaWduKG9iaiwgeyBiYXI6ICdkd2EnIH0sIHsgdHJ6eTogJ3RyenknIH0pO1xuXHRyZXR1cm4gKG9iai5mb28gKyBvYmouYmFyICsgb2JqLnRyenkpID09PSAncmF6ZHdhdHJ6eSc7XG59O1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG52YXIga2V5cyAgPSByZXF1aXJlKCcuLi9rZXlzJylcbiAgLCB2YWx1ZSA9IHJlcXVpcmUoJy4uL3ZhbGlkLXZhbHVlJylcblxuICAsIG1heCA9IE1hdGgubWF4O1xuXG5tb2R1bGUuZXhwb3J0cyA9IGZ1bmN0aW9uIChkZXN0LCBzcmMvKiwg4oCmc3JjbiovKSB7XG5cdHZhciBlcnJvciwgaSwgbCA9IG1heChhcmd1bWVudHMubGVuZ3RoLCAyKSwgYXNzaWduO1xuXHRkZXN0ID0gT2JqZWN0KHZhbHVlKGRlc3QpKTtcblx0YXNzaWduID0gZnVuY3Rpb24gKGtleSkge1xuXHRcdHRyeSB7IGRlc3Rba2V5XSA9IHNyY1trZXldOyB9IGNhdGNoIChlKSB7XG5cdFx0XHRpZiAoIWVycm9yKSBlcnJvciA9IGU7XG5cdFx0fVxuXHR9O1xuXHRmb3IgKGkgPSAxOyBpIDwgbDsgKytpKSB7XG5cdFx0c3JjID0gYXJndW1lbnRzW2ldO1xuXHRcdGtleXMoc3JjKS5mb3JFYWNoKGFzc2lnbik7XG5cdH1cblx0aWYgKGVycm9yICE9PSB1bmRlZmluZWQpIHRocm93IGVycm9yO1xuXHRyZXR1cm4gZGVzdDtcbn07XG4iLCIvLyBEZXByZWNhdGVkXG5cbid1c2Ugc3RyaWN0JztcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAob2JqKSB7IHJldHVybiB0eXBlb2Ygb2JqID09PSAnZnVuY3Rpb24nOyB9O1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG5tb2R1bGUuZXhwb3J0cyA9IHJlcXVpcmUoJy4vaXMtaW1wbGVtZW50ZWQnKSgpXG5cdD8gT2JqZWN0LmtleXNcblx0OiByZXF1aXJlKCcuL3NoaW0nKTtcbiIsIid1c2Ugc3RyaWN0JztcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAoKSB7XG5cdHRyeSB7XG5cdFx0T2JqZWN0LmtleXMoJ3ByaW1pdGl2ZScpO1xuXHRcdHJldHVybiB0cnVlO1xuXHR9IGNhdGNoIChlKSB7IHJldHVybiBmYWxzZTsgfVxufTtcbiIsIid1c2Ugc3RyaWN0JztcblxudmFyIGtleXMgPSBPYmplY3Qua2V5cztcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAob2JqZWN0KSB7XG5cdHJldHVybiBrZXlzKG9iamVjdCA9PSBudWxsID8gb2JqZWN0IDogT2JqZWN0KG9iamVjdCkpO1xufTtcbiIsIid1c2Ugc3RyaWN0JztcblxudmFyIGFzc2lnbiA9IHJlcXVpcmUoJy4vYXNzaWduJylcblxuICAsIGZvckVhY2ggPSBBcnJheS5wcm90b3R5cGUuZm9yRWFjaFxuICAsIGNyZWF0ZSA9IE9iamVjdC5jcmVhdGUsIGdldFByb3RvdHlwZU9mID0gT2JqZWN0LmdldFByb3RvdHlwZU9mXG5cbiAgLCBwcm9jZXNzO1xuXG5wcm9jZXNzID0gZnVuY3Rpb24gKHNyYywgb2JqKSB7XG5cdHZhciBwcm90byA9IGdldFByb3RvdHlwZU9mKHNyYyk7XG5cdHJldHVybiBhc3NpZ24ocHJvdG8gPyBwcm9jZXNzKHByb3RvLCBvYmopIDogb2JqLCBzcmMpO1xufTtcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAob3B0aW9ucy8qLCDigKZvcHRpb25zKi8pIHtcblx0dmFyIHJlc3VsdCA9IGNyZWF0ZShudWxsKTtcblx0Zm9yRWFjaC5jYWxsKGFyZ3VtZW50cywgZnVuY3Rpb24gKG9wdGlvbnMpIHtcblx0XHRpZiAob3B0aW9ucyA9PSBudWxsKSByZXR1cm47XG5cdFx0cHJvY2VzcyhPYmplY3Qob3B0aW9ucyksIHJlc3VsdCk7XG5cdH0pO1xuXHRyZXR1cm4gcmVzdWx0O1xufTtcbiIsIid1c2Ugc3RyaWN0JztcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAodmFsdWUpIHtcblx0aWYgKHZhbHVlID09IG51bGwpIHRocm93IG5ldyBUeXBlRXJyb3IoXCJDYW5ub3QgdXNlIG51bGwgb3IgdW5kZWZpbmVkXCIpO1xuXHRyZXR1cm4gdmFsdWU7XG59O1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG5tb2R1bGUuZXhwb3J0cyA9IHJlcXVpcmUoJy4vaXMtaW1wbGVtZW50ZWQnKSgpXG5cdD8gU3RyaW5nLnByb3RvdHlwZS5jb250YWluc1xuXHQ6IHJlcXVpcmUoJy4vc2hpbScpO1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG52YXIgc3RyID0gJ3JhemR3YXRyenknO1xuXG5tb2R1bGUuZXhwb3J0cyA9IGZ1bmN0aW9uICgpIHtcblx0aWYgKHR5cGVvZiBzdHIuY29udGFpbnMgIT09ICdmdW5jdGlvbicpIHJldHVybiBmYWxzZTtcblx0cmV0dXJuICgoc3RyLmNvbnRhaW5zKCdkd2EnKSA9PT0gdHJ1ZSkgJiYgKHN0ci5jb250YWlucygnZm9vJykgPT09IGZhbHNlKSk7XG59O1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG52YXIgaW5kZXhPZiA9IFN0cmluZy5wcm90b3R5cGUuaW5kZXhPZjtcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAoc2VhcmNoU3RyaW5nLyosIHBvc2l0aW9uKi8pIHtcblx0cmV0dXJuIGluZGV4T2YuY2FsbCh0aGlzLCBzZWFyY2hTdHJpbmcsIGFyZ3VtZW50c1sxXSkgPiAtMTtcbn07XG4iLCIndXNlIHN0cmljdCc7XG5cbnZhciBkID0gcmVxdWlyZSgnZCcpXG5cbiAgLCBjcmVhdGUgPSBPYmplY3QuY3JlYXRlLCBkZWZpbmVQcm9wZXJ0aWVzID0gT2JqZWN0LmRlZmluZVByb3BlcnRpZXNcbiAgLCBnZW5lcmF0ZU5hbWUsIFN5bWJvbDtcblxuZ2VuZXJhdGVOYW1lID0gKGZ1bmN0aW9uICgpIHtcblx0dmFyIGNyZWF0ZWQgPSBjcmVhdGUobnVsbCk7XG5cdHJldHVybiBmdW5jdGlvbiAoZGVzYykge1xuXHRcdHZhciBwb3N0Zml4ID0gMDtcblx0XHR3aGlsZSAoY3JlYXRlZFtkZXNjICsgKHBvc3RmaXggfHwgJycpXSkgKytwb3N0Zml4O1xuXHRcdGRlc2MgKz0gKHBvc3RmaXggfHwgJycpO1xuXHRcdGNyZWF0ZWRbZGVzY10gPSB0cnVlO1xuXHRcdHJldHVybiAnQEAnICsgZGVzYztcblx0fTtcbn0oKSk7XG5cbm1vZHVsZS5leHBvcnRzID0gU3ltYm9sID0gZnVuY3Rpb24gKGRlc2NyaXB0aW9uKSB7XG5cdHZhciBzeW1ib2w7XG5cdGlmICh0aGlzIGluc3RhbmNlb2YgU3ltYm9sKSB7XG5cdFx0dGhyb3cgbmV3IFR5cGVFcnJvcignVHlwZUVycm9yOiBTeW1ib2wgaXMgbm90IGEgY29uc3RydWN0b3InKTtcblx0fVxuXHRzeW1ib2wgPSBjcmVhdGUoU3ltYm9sLnByb3RvdHlwZSk7XG5cdGRlc2NyaXB0aW9uID0gKGRlc2NyaXB0aW9uID09PSB1bmRlZmluZWQgPyAnJyA6IFN0cmluZyhkZXNjcmlwdGlvbikpO1xuXHRyZXR1cm4gZGVmaW5lUHJvcGVydGllcyhzeW1ib2wsIHtcblx0XHRfX2Rlc2NyaXB0aW9uX186IGQoJycsIGRlc2NyaXB0aW9uKSxcblx0XHRfX25hbWVfXzogZCgnJywgZ2VuZXJhdGVOYW1lKGRlc2NyaXB0aW9uKSlcblx0fSk7XG59O1xuXG5PYmplY3QuZGVmaW5lUHJvcGVydGllcyhTeW1ib2wsIHtcblx0Y3JlYXRlOiBkKCcnLCBTeW1ib2woJ2NyZWF0ZScpKSxcblx0aGFzSW5zdGFuY2U6IGQoJycsIFN5bWJvbCgnaGFzSW5zdGFuY2UnKSksXG5cdGlzQ29uY2F0U3ByZWFkYWJsZTogZCgnJywgU3ltYm9sKCdpc0NvbmNhdFNwcmVhZGFibGUnKSksXG5cdGlzUmVnRXhwOiBkKCcnLCBTeW1ib2woJ2lzUmVnRXhwJykpLFxuXHRpdGVyYXRvcjogZCgnJywgU3ltYm9sKCdpdGVyYXRvcicpKSxcblx0dG9QcmltaXRpdmU6IGQoJycsIFN5bWJvbCgndG9QcmltaXRpdmUnKSksXG5cdHRvU3RyaW5nVGFnOiBkKCcnLCBTeW1ib2woJ3RvU3RyaW5nVGFnJykpLFxuXHR1bnNjb3BhYmxlczogZCgnJywgU3ltYm9sKCd1bnNjb3BhYmxlcycpKVxufSk7XG5cbmRlZmluZVByb3BlcnRpZXMoU3ltYm9sLnByb3RvdHlwZSwge1xuXHRwcm9wZXJUb1N0cmluZzogZChmdW5jdGlvbiAoKSB7XG5cdFx0cmV0dXJuICdTeW1ib2wgKCcgKyB0aGlzLl9fZGVzY3JpcHRpb25fXyArICcpJztcblx0fSksXG5cdHRvU3RyaW5nOiBkKCcnLCBmdW5jdGlvbiAoKSB7IHJldHVybiB0aGlzLl9fbmFtZV9fOyB9KVxufSk7XG5PYmplY3QuZGVmaW5lUHJvcGVydHkoU3ltYm9sLnByb3RvdHlwZSwgU3ltYm9sLnRvUHJpbWl0aXZlLCBkKCcnLFxuXHRmdW5jdGlvbiAoaGludCkge1xuXHRcdHRocm93IG5ldyBUeXBlRXJyb3IoXCJDb252ZXJzaW9uIG9mIHN5bWJvbCBvYmplY3RzIGlzIG5vdCBhbGxvd2VkXCIpO1xuXHR9KSk7XG5PYmplY3QuZGVmaW5lUHJvcGVydHkoU3ltYm9sLnByb3RvdHlwZSwgU3ltYm9sLnRvU3RyaW5nVGFnLCBkKCdjJywgJ1N5bWJvbCcpKTtcbiIsIm1vZHVsZS5leHBvcnRzPXtcbiAgXCJ0eXBlc1wiOiB7XG4gICAgXCJyb290XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInJvb3RcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJlY2hvXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwic3RyaW5nXCI6IHsgXCJfYXJnXCI6IDAsIFwidHlwZVwiOiBcInN0cmluZ1wiIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzdHJpbmdcIjogeyBcIl9yZXR2YWxcIjogXCJzdHJpbmdcIiB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwibGlzdFRhYnNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge30sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7IFwiX3JldHZhbFwiOiBcInRhYmxpc3RcIiB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJwcm90b2NvbERlc2NyaXB0aW9uXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHt9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjogeyBcIl9yZXR2YWxcIjogXCJqc29uXCIgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge1xuICAgICAgICBcInRhYkxpc3RDaGFuZ2VkXCI6IHt9XG4gICAgICB9XG4gICAgfSxcbiAgICBcInRhYmxpc3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJ0YWJsaXN0XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwic2VsZWN0ZWRcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJ0YWJzXCI6IFwiYXJyYXk6dGFiXCIsXG4gICAgICAgIFwidXJsXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwiY29uc29sZUFjdG9yXCI6IFwiY29uc29sZVwiLFxuICAgICAgICBcImluc3BlY3RvckFjdG9yXCI6IFwiaW5zcGVjdG9yXCIsXG4gICAgICAgIFwic3R5bGVTaGVldHNBY3RvclwiOiBcInN0eWxlc2hlZXRzXCIsXG4gICAgICAgIFwic3R5bGVFZGl0b3JBY3RvclwiOiBcInN0eWxlZWRpdG9yXCIsXG4gICAgICAgIFwibWVtb3J5QWN0b3JcIjogXCJtZW1vcnlcIixcbiAgICAgICAgXCJldmVudExvb3BMYWdBY3RvclwiOiBcImV2ZW50TG9vcExhZ1wiLFxuICAgICAgICBcInByZWZlcmVuY2VBY3RvclwiOiBcInByZWZlcmVuY2VcIixcbiAgICAgICAgXCJkZXZpY2VBY3RvclwiOiBcImRldmljZVwiLFxuXG4gICAgICAgIFwicHJvZmlsZXJBY3RvclwiOiBcInByb2ZpbGVyXCIsXG4gICAgICAgIFwiY2hyb21lRGVidWdnZXJcIjogXCJjaHJvbWVEZWJ1Z2dlclwiLFxuICAgICAgICBcIndlYmFwcHNBY3RvclwiOiBcIndlYmFwcHNcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJ0YWJcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwidGFiXCIsXG4gICAgICBcImZpZWxkc1wiOiB7XG4gICAgICAgIFwidGl0bGVcIjogXCJzdHJpbmdcIixcbiAgICAgICAgXCJ1cmxcIjogXCJzdHJpbmdcIixcbiAgICAgICAgXCJvdXRlcldpbmRvd0lEXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwiaW5zcGVjdG9yQWN0b3JcIjogXCJpbnNwZWN0b3JcIixcbiAgICAgICAgXCJjYWxsV2F0Y2hlckFjdG9yXCI6IFwiY2FsbC13YXRjaGVyXCIsXG4gICAgICAgIFwiY2FudmFzQWN0b3JcIjogXCJjYW52YXNcIixcbiAgICAgICAgXCJ3ZWJnbEFjdG9yXCI6IFwid2ViZ2xcIixcbiAgICAgICAgXCJ3ZWJhdWRpb0FjdG9yXCI6IFwid2ViYXVkaW9cIixcbiAgICAgICAgXCJzdG9yYWdlQWN0b3JcIjogXCJzdG9yYWdlXCIsXG4gICAgICAgIFwiZ2NsaUFjdG9yXCI6IFwiZ2NsaVwiLFxuICAgICAgICBcIm1lbW9yeUFjdG9yXCI6IFwibWVtb3J5XCIsXG4gICAgICAgIFwiZXZlbnRMb29wTGFnXCI6IFwiZXZlbnRMb29wTGFnXCIsXG4gICAgICAgIFwic3R5bGVTaGVldHNBY3RvclwiOiBcInN0eWxlc2hlZXRzXCIsXG4gICAgICAgIFwic3R5bGVFZGl0b3JBY3RvclwiOiBcInN0eWxlZWRpdG9yXCIsXG5cbiAgICAgICAgXCJjb25zb2xlQWN0b3JcIjogXCJjb25zb2xlXCIsXG4gICAgICAgIFwidHJhY2VBY3RvclwiOiBcInRyYWNlXCJcbiAgICAgIH1cbiAgICB9XG4gIH1cbn1cbiIsIm1vZHVsZS5leHBvcnRzPXtcbiAgXCJ0eXBlc1wiOiB7XG4gICAgXCJsb25nc3RyYWN0b3JcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwibG9uZ3N0cmFjdG9yXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic3Vic3RyaW5nXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN1YnN0cmluZ1wiLFxuICAgICAgICAgICAgXCJzdGFydFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwiZW5kXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic3Vic3RyaW5nXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJyZWxlYXNlXCIsXG4gICAgICAgICAgXCJyZWxlYXNlXCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlbGVhc2VcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwic3R5bGVzaGVldFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJzdHlsZXNoZWV0XCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwidG9nZ2xlRGlzYWJsZWRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwidG9nZ2xlRGlzYWJsZWRcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcImRpc2FibGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0VGV4dFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRUZXh0XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ0ZXh0XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibG9uZ3N0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0T3JpZ2luYWxTb3VyY2VzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldE9yaWdpbmFsU291cmNlc1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwib3JpZ2luYWxTb3VyY2VzXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibnVsbGFibGU6YXJyYXk6b3JpZ2luYWxzb3VyY2VcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldE9yaWdpbmFsTG9jYXRpb25cIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0T3JpZ2luYWxMb2NhdGlvblwiLFxuICAgICAgICAgICAgXCJsaW5lXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bWJlclwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJjb2x1bW5cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVtYmVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwib3JpZ2luYWxsb2NhdGlvbnJlc3BvbnNlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ1cGRhdGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwidXBkYXRlXCIsXG4gICAgICAgICAgICBcInRleHRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInRyYW5zaXRpb25cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7XG4gICAgICAgIFwicHJvcGVydHktY2hhbmdlXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJwcm9wZXJ0eUNoYW5nZVwiLFxuICAgICAgICAgIFwicHJvcGVydHlcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImpzb25cIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJzdHlsZS1hcHBsaWVkXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJzdHlsZUFwcGxpZWRcIlxuICAgICAgICB9XG4gICAgICB9XG4gICAgfSxcbiAgICBcIm9yaWdpbmFsc291cmNlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcIm9yaWdpbmFsc291cmNlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0VGV4dFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRUZXh0XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ0ZXh0XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibG9uZ3N0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwic3R5bGVzaGVldHNcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwic3R5bGVzaGVldHNcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRTdHlsZVNoZWV0c1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRTdHlsZVNoZWV0c1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic3R5bGVTaGVldHNcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJhcnJheTpzdHlsZXNoZWV0XCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJhZGRTdHlsZVNoZWV0XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImFkZFN0eWxlU2hlZXRcIixcbiAgICAgICAgICAgIFwidGV4dFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInN0eWxlU2hlZXRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzdHlsZXNoZWV0XCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJvcmlnaW5hbGxvY2F0aW9ucmVzcG9uc2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJvcmlnaW5hbGxvY2F0aW9ucmVzcG9uc2VcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJzb3VyY2VcIjogXCJzdHJpbmdcIixcbiAgICAgICAgXCJsaW5lXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwiY29sdW1uXCI6IFwibnVtYmVyXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiZG9tbm9kZVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJkb21ub2RlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0Tm9kZVZhbHVlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldE5vZGVWYWx1ZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJsb25nc3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZXROb2RlVmFsdWVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2V0Tm9kZVZhbHVlXCIsXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRJbWFnZURhdGFcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0SW1hZ2VEYXRhXCIsXG4gICAgICAgICAgICBcIm1heERpbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpudW1iZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJpbWFnZURhdGFcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIm1vZGlmeUF0dHJpYnV0ZXNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwibW9kaWZ5QXR0cmlidXRlc1wiLFxuICAgICAgICAgICAgXCJtb2RpZmljYXRpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImFycmF5Ompzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiYXBwbGllZHN0eWxlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiYXBwbGllZHN0eWxlXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwicnVsZVwiOiBcImRvbXN0eWxlcnVsZSNhY3RvcmlkXCIsXG4gICAgICAgIFwiaW5oZXJpdGVkXCI6IFwibnVsbGFibGU6ZG9tbm9kZSNhY3RvcmlkXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwibWF0Y2hlZHNlbGVjdG9yXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwibWF0Y2hlZHNlbGVjdG9yXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwicnVsZVwiOiBcImRvbXN0eWxlcnVsZSNhY3RvcmlkXCIsXG4gICAgICAgIFwic2VsZWN0b3JcIjogXCJzdHJpbmdcIixcbiAgICAgICAgXCJ2YWx1ZVwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcInN0YXR1c1wiOiBcIm51bWJlclwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcIm1hdGNoZWRzZWxlY3RvcnJlc3BvbnNlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwibWF0Y2hlZHNlbGVjdG9ycmVzcG9uc2VcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJydWxlc1wiOiBcImFycmF5OmRvbXN0eWxlcnVsZVwiLFxuICAgICAgICBcInNoZWV0c1wiOiBcImFycmF5OnN0eWxlc2hlZXRcIixcbiAgICAgICAgXCJtYXRjaGVkXCI6IFwiYXJyYXk6bWF0Y2hlZHNlbGVjdG9yXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiYXBwbGllZFN0eWxlc1JldHVyblwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImFwcGxpZWRTdHlsZXNSZXR1cm5cIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJlbnRyaWVzXCI6IFwiYXJyYXk6YXBwbGllZHN0eWxlXCIsXG4gICAgICAgIFwicnVsZXNcIjogXCJhcnJheTpkb21zdHlsZXJ1bGVcIixcbiAgICAgICAgXCJzaGVldHNcIjogXCJhcnJheTpzdHlsZXNoZWV0XCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwicGFnZXN0eWxlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInBhZ2VzdHlsZVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldENvbXB1dGVkXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldENvbXB1dGVkXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJtYXJrTWF0Y2hlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm9ubHlNYXRjaGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwiZmlsdGVyXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiY29tcHV0ZWRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRNYXRjaGVkU2VsZWN0b3JzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldE1hdGNoZWRTZWxlY3RvcnNcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInByb3BlcnR5XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJmaWx0ZXJcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibWF0Y2hlZHNlbGVjdG9ycmVzcG9uc2VcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldEFwcGxpZWRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0QXBwbGllZFwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwiaW5oZXJpdGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwibWF0Y2hlZFNlbGVjdG9yc1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImZpbHRlclwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJhcHBsaWVkU3R5bGVzUmV0dXJuXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRMYXlvdXRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0TGF5b3V0XCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJhdXRvTWFyZ2luc1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiZG9tc3R5bGVydWxlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImRvbXN0eWxlcnVsZVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIm1vZGlmeVByb3BlcnRpZXNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwibW9kaWZ5UHJvcGVydGllc1wiLFxuICAgICAgICAgICAgXCJtb2RpZmljYXRpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImFycmF5Ompzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInJ1bGVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJkb21zdHlsZXJ1bGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcImhpZ2hsaWdodGVyXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImhpZ2hsaWdodGVyXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2hvd0JveE1vZGVsXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNob3dCb3hNb2RlbFwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicmVnaW9uXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJoaWRlQm94TW9kZWxcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaGlkZUJveE1vZGVsXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInBpY2tcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicGlja1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJjYW5jZWxQaWNrXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImNhbmNlbFBpY2tcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiaW1hZ2VEYXRhXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiaW1hZ2VEYXRhXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwiZGF0YVwiOiBcIm51bGxhYmxlOmxvbmdzdHJpbmdcIixcbiAgICAgICAgXCJzaXplXCI6IFwianNvblwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcImRpc2Nvbm5lY3RlZE5vZGVcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJkaXNjb25uZWN0ZWROb2RlXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwibm9kZVwiOiBcImRvbW5vZGVcIixcbiAgICAgICAgXCJuZXdQYXJlbnRzXCI6IFwiYXJyYXk6ZG9tbm9kZVwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcImRpc2Nvbm5lY3RlZE5vZGVBcnJheVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImRpc2Nvbm5lY3RlZE5vZGVBcnJheVwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcIm5vZGVzXCI6IFwiYXJyYXk6ZG9tbm9kZVwiLFxuICAgICAgICBcIm5ld1BhcmVudHNcIjogXCJhcnJheTpkb21ub2RlXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiZG9tbXV0YXRpb25cIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJkb21tdXRhdGlvblwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge31cbiAgICB9LFxuICAgIFwiZG9tbm9kZWxpc3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZG9tbm9kZWxpc3RcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJpdGVtXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcIml0ZW1cIixcbiAgICAgICAgICAgIFwiaXRlbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJkaXNjb25uZWN0ZWROb2RlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJpdGVtc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJpdGVtc1wiLFxuICAgICAgICAgICAgXCJzdGFydFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpudW1iZXJcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwiZW5kXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOm51bWJlclwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRpc2Nvbm5lY3RlZE5vZGVBcnJheVwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicmVsZWFzZVwiLFxuICAgICAgICAgIFwicmVsZWFzZVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJyZWxlYXNlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcImRvbXRyYXZlcnNhbGFycmF5XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZG9tdHJhdmVyc2FsYXJyYXlcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJub2Rlc1wiOiBcImFycmF5OmRvbW5vZGVcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJkb213YWxrZXJcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZG9td2Fsa2VyXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicmVsZWFzZVwiLFxuICAgICAgICAgIFwicmVsZWFzZVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJyZWxlYXNlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInBpY2tcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicGlja1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRpc2Nvbm5lY3RlZE5vZGVcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImNhbmNlbFBpY2tcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiY2FuY2VsUGlja1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJoaWdobGlnaHRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaGlnaGxpZ2h0XCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6ZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJkb2N1bWVudFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJkb2N1bWVudFwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJkb2N1bWVudEVsZW1lbnRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9jdW1lbnRFbGVtZW50XCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6ZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInBhcmVudHNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicGFyZW50c1wiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwic2FtZURvY3VtZW50XCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibm9kZXNcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJhcnJheTpkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJyZXRhaW5Ob2RlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJldGFpbk5vZGVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInVucmV0YWluTm9kZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ1bnJldGFpbk5vZGVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInJlbGVhc2VOb2RlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlbGVhc2VOb2RlXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJmb3JjZVwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiY2hpbGRyZW5cIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiY2hpbGRyZW5cIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm1heE5vZGVzXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJjZW50ZXJcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzdGFydFwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIndoYXRUb1Nob3dcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZG9tdHJhdmVyc2FsYXJyYXlcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNpYmxpbmdzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNpYmxpbmdzXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJtYXhOb2Rlc1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwiY2VudGVyXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwic3RhcnRcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJ3aGF0VG9TaG93XCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRvbXRyYXZlcnNhbGFycmF5XCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJuZXh0U2libGluZ1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJuZXh0U2libGluZ1wiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwid2hhdFRvU2hvd1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTpkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJwcmV2aW91c1NpYmxpbmdcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJldmlvdXNTaWJsaW5nXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJ3aGF0VG9TaG93XCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bGxhYmxlOmRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInF1ZXJ5U2VsZWN0b3JcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicXVlcnlTZWxlY3RvclwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwic2VsZWN0b3JcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZGlzY29ubmVjdGVkTm9kZVwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicXVlcnlTZWxlY3RvckFsbFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJxdWVyeVNlbGVjdG9yQWxsXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzZWxlY3RvclwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcImxpc3RcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJkb21ub2RlbGlzdFwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0U3VnZ2VzdGlvbnNGb3JRdWVyeVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRTdWdnZXN0aW9uc0ZvclF1ZXJ5XCIsXG4gICAgICAgICAgICBcInF1ZXJ5XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJjb21wbGV0aW5nXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzZWxlY3RvclN0YXRlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDIsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibGlzdFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImFycmF5OmFycmF5OnN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiYWRkUHNldWRvQ2xhc3NMb2NrXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImFkZFBzZXVkb0NsYXNzTG9ja1wiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicHNldWRvQ2xhc3NcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInBhcmVudHNcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImhpZGVOb2RlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImhpZGVOb2RlXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ1bmhpZGVOb2RlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInVuaGlkZU5vZGVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInJlbW92ZVBzZXVkb0NsYXNzTG9ja1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJyZW1vdmVQc2V1ZG9DbGFzc0xvY2tcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInBzZXVkb0NsYXNzXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJwYXJlbnRzXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDIsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJjbGVhclBzZXVkb0NsYXNzTG9ja3NcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiY2xlYXJQc2V1ZG9DbGFzc0xvY2tzXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6ZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJpbm5lckhUTUxcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaW5uZXJIVE1MXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJsb25nc3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJvdXRlckhUTUxcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwib3V0ZXJIVE1MXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJsb25nc3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZXRPdXRlckhUTUxcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2V0T3V0ZXJIVE1MXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicmVtb3ZlTm9kZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJyZW1vdmVOb2RlXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibmV4dFNpYmxpbmdcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTpkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJpbnNlcnRCZWZvcmVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaW5zZXJ0QmVmb3JlXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJwYXJlbnRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzaWJsaW5nXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDIsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0TXV0YXRpb25zXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldE11dGF0aW9uc1wiLFxuICAgICAgICAgICAgXCJjbGVhbnVwXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibXV0YXRpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYXJyYXk6ZG9tbXV0YXRpb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImlzSW5ET01UcmVlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImlzSW5ET01UcmVlXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiYXR0YWNoZWRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXROb2RlQWN0b3JGcm9tT2JqZWN0QWN0b3JcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0Tm9kZUFjdG9yRnJvbU9iamVjdEFjdG9yXCIsXG4gICAgICAgICAgICBcIm9iamVjdEFjdG9ySURcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJub2RlRnJvbnRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTpkaXNjb25uZWN0ZWROb2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7XG4gICAgICAgIFwibmV3LW11dGF0aW9uc1wiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwibmV3TXV0YXRpb25zXCJcbiAgICAgICAgfSxcbiAgICAgICAgXCJwaWNrZXItbm9kZS1waWNrZWRcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcInBpY2tlck5vZGVQaWNrZWRcIixcbiAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJkaXNjb25uZWN0ZWROb2RlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwicGlja2VyLW5vZGUtaG92ZXJlZFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwicGlja2VyTm9kZUhvdmVyZWRcIixcbiAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJkaXNjb25uZWN0ZWROb2RlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwiaGlnaGxpZ2h0ZXItcmVhZHlcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcImhpZ2hsaWdodGVyLXJlYWR5XCJcbiAgICAgICAgfSxcbiAgICAgICAgXCJoaWdobGlnaHRlci1oaWRlXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJoaWdobGlnaHRlci1oaWRlXCJcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJpbnNwZWN0b3JcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiaW5zcGVjdG9yXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0V2Fsa2VyXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFdhbGtlclwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwid2Fsa2VyXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZG9td2Fsa2VyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRQYWdlU3R5bGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0UGFnZVN0eWxlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJwYWdlU3R5bGVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJwYWdlc3R5bGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldEhpZ2hsaWdodGVyXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldEhpZ2hsaWdodGVyXCIsXG4gICAgICAgICAgICBcImF1dG9oaWRlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcImhpZ2hsaWd0ZXJcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJoaWdobGlnaHRlclwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0SW1hZ2VEYXRhRnJvbVVSTFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRJbWFnZURhdGFGcm9tVVJMXCIsXG4gICAgICAgICAgICBcInVybFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwibWF4RGltXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOm51bWJlclwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImltYWdlRGF0YVwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiY2FsbC1zdGFjay1pdGVtXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiY2FsbC1zdGFjay1pdGVtXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwibmFtZVwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcImZpbGVcIjogXCJzdHJpbmdcIixcbiAgICAgICAgXCJsaW5lXCI6IFwibnVtYmVyXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiY2FsbC1kZXRhaWxzXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiY2FsbC1kZXRhaWxzXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwidHlwZVwiOiBcIm51bWJlclwiLFxuICAgICAgICBcIm5hbWVcIjogXCJzdHJpbmdcIixcbiAgICAgICAgXCJzdGFja1wiOiBcImFycmF5OmNhbGwtc3RhY2staXRlbVwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcImZ1bmN0aW9uLWNhbGxcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZnVuY3Rpb24tY2FsbFwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldERldGFpbHNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0RGV0YWlsc1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiaW5mb1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImNhbGwtZGV0YWlsc1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiY2FsbC13YXRjaGVyXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImNhbGwtd2F0Y2hlclwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNldHVwXCIsXG4gICAgICAgICAgXCJvbmV3YXlcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2V0dXBcIixcbiAgICAgICAgICAgIFwidHJhY2VkR2xvYmFsc1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTphcnJheTpzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwidHJhY2VkRnVuY3Rpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmFycmF5OnN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzdGFydFJlY29yZGluZ1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInBlcmZvcm1SZWxvYWRcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJmaW5hbGl6ZVwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImZpbmFsaXplXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImlzUmVjb3JkaW5nXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImlzUmVjb3JkaW5nXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicmVzdW1lUmVjb3JkaW5nXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlc3VtZVJlY29yZGluZ1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJwYXVzZVJlY29yZGluZ1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJwYXVzZVJlY29yZGluZ1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiY2FsbHNcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJhcnJheTpmdW5jdGlvbi1jYWxsXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJlcmFzZVJlY29yZGluZ1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJlcmFzZVJlY29yZGluZ1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJzbmFwc2hvdC1pbWFnZVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInNuYXBzaG90LWltYWdlXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwiaW5kZXhcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJ3aWR0aFwiOiBcIm51bWJlclwiLFxuICAgICAgICBcImhlaWdodFwiOiBcIm51bWJlclwiLFxuICAgICAgICBcImZsaXBwZWRcIjogXCJib29sZWFuXCIsXG4gICAgICAgIFwicGl4ZWxzXCI6IFwidWludDMyLWFycmF5XCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwic25hcHNob3Qtb3ZlcnZpZXdcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJzbmFwc2hvdC1vdmVydmlld1wiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcImNhbGxzXCI6IFwiYXJyYXk6ZnVuY3Rpb24tY2FsbFwiLFxuICAgICAgICBcInRodW1ibmFpbHNcIjogXCJhcnJheTpzbmFwc2hvdC1pbWFnZVwiLFxuICAgICAgICBcInNjcmVlbnNob3RcIjogXCJzbmFwc2hvdC1pbWFnZVwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcImZyYW1lLXNuYXBzaG90XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImZyYW1lLXNuYXBzaG90XCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0T3ZlcnZpZXdcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0T3ZlcnZpZXdcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIm92ZXJ2aWV3XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic25hcHNob3Qtb3ZlcnZpZXdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdlbmVyYXRlU2NyZWVuc2hvdEZvclwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZW5lcmF0ZVNjcmVlbnNob3RGb3JcIixcbiAgICAgICAgICAgIFwiY2FsbFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJmdW5jdGlvbi1jYWxsXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzY3JlZW5zaG90XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic25hcHNob3QtaW1hZ2VcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcImNhbnZhc1wiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJjYW52YXNcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZXR1cFwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNldHVwXCIsXG4gICAgICAgICAgICBcInJlbG9hZFwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImZpbmFsaXplXCIsXG4gICAgICAgICAgXCJvbmV3YXlcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZmluYWxpemVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiaXNJbml0aWFsaXplZFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJpc0luaXRpYWxpemVkXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJpbml0aWFsaXplZFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInJlY29yZEFuaW1hdGlvbkZyYW1lXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlY29yZEFuaW1hdGlvbkZyYW1lXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzbmFwc2hvdFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImZyYW1lLXNuYXBzaG90XCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJnbC1zaGFkZXJcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZ2wtc2hhZGVyXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0VGV4dFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRUZXh0XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ0ZXh0XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJjb21waWxlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImNvbXBpbGVcIixcbiAgICAgICAgICAgIFwidGV4dFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcImVycm9yXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibnVsbGFibGU6anNvblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiZ2wtcHJvZ3JhbVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJnbC1wcm9ncmFtXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0VmVydGV4U2hhZGVyXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFZlcnRleFNoYWRlclwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic2hhZGVyXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZ2wtc2hhZGVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRGcmFnbWVudFNoYWRlclwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRGcmFnbWVudFNoYWRlclwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic2hhZGVyXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZ2wtc2hhZGVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJoaWdobGlnaHRcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJoaWdobGlnaHRcIixcbiAgICAgICAgICAgIFwidGludFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJhcnJheTpudW1iZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwidW5oaWdobGlnaHRcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ1bmhpZ2hsaWdodFwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJibGFja2JveFwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImJsYWNrYm94XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInVuYmxhY2tib3hcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ1bmJsYWNrYm94XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcIndlYmdsXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcIndlYmdsXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2V0dXBcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXR1cFwiLFxuICAgICAgICAgICAgXCJyZWxvYWRcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJmaW5hbGl6ZVwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImZpbmFsaXplXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFByb2dyYW1zXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFByb2dyYW1zXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJwcm9ncmFtc1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImFycmF5OmdsLXByb2dyYW1cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJwcm9ncmFtLWxpbmtlZFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwicHJvZ3JhbUxpbmtlZFwiLFxuICAgICAgICAgIFwicHJvZ3JhbVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdsLXByb2dyYW1cIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJhdWRpb25vZGVcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiYXVkaW9ub2RlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0VHlwZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRUeXBlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJpc1NvdXJjZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJpc1NvdXJjZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic291cmNlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2V0UGFyYW1cIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2V0UGFyYW1cIixcbiAgICAgICAgICAgIFwicGFyYW1cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOnByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiZXJyb3JcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTpqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRQYXJhbVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRQYXJhbVwiLFxuICAgICAgICAgICAgXCJwYXJhbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInRleHRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTpwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFBhcmFtRmxhZ3NcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0UGFyYW1GbGFnc1wiLFxuICAgICAgICAgICAgXCJwYXJhbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcImZsYWdzXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibnVsbGFibGU6cHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRQYXJhbXNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0UGFyYW1zXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJwYXJhbXNcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJ3ZWJhdWRpb1wiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJ3ZWJhdWRpb1wiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNldHVwXCIsXG4gICAgICAgICAgXCJvbmV3YXlcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2V0dXBcIixcbiAgICAgICAgICAgIFwicmVsb2FkXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZmluYWxpemVcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJmaW5hbGl6ZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7XG4gICAgICAgIFwic3RhcnQtY29udGV4dFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwic3RhcnRDb250ZXh0XCJcbiAgICAgICAgfSxcbiAgICAgICAgXCJjb25uZWN0LW5vZGVcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcImNvbm5lY3ROb2RlXCIsXG4gICAgICAgICAgXCJzb3VyY2VcIjoge1xuICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJhdWRpb25vZGVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJkZXN0XCI6IHtcbiAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYXVkaW9ub2RlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwiZGlzY29ubmVjdC1ub2RlXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJkaXNjb25uZWN0Tm9kZVwiLFxuICAgICAgICAgIFwic291cmNlXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYXVkaW9ub2RlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwiY29ubmVjdC1wYXJhbVwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwiY29ubmVjdFBhcmFtXCIsXG4gICAgICAgICAgXCJzb3VyY2VcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJhdWRpb25vZGVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJwYXJhbVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICBcImNoYW5nZS1wYXJhbVwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwiY2hhbmdlUGFyYW1cIixcbiAgICAgICAgICBcInNvdXJjZVwiOiB7XG4gICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImF1ZGlvbm9kZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInBhcmFtXCI6IHtcbiAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJjcmVhdGUtbm9kZVwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwiY3JlYXRlTm9kZVwiLFxuICAgICAgICAgIFwic291cmNlXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYXVkaW9ub2RlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIH1cbiAgICB9LFxuICAgIFwib2xkLXN0eWxlc2hlZXRcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwib2xkLXN0eWxlc2hlZXRcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ0b2dnbGVEaXNhYmxlZFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ0b2dnbGVEaXNhYmxlZFwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiZGlzYWJsZWRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJmZXRjaFNvdXJjZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJmZXRjaFNvdXJjZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ1cGRhdGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwidXBkYXRlXCIsXG4gICAgICAgICAgICBcInRleHRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInRyYW5zaXRpb25cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7XG4gICAgICAgIFwicHJvcGVydHktY2hhbmdlXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJwcm9wZXJ0eUNoYW5nZVwiLFxuICAgICAgICAgIFwicHJvcGVydHlcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImpzb25cIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJzb3VyY2UtbG9hZFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwic291cmNlTG9hZFwiLFxuICAgICAgICAgIFwic291cmNlXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwic3R5bGUtYXBwbGllZFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwic3R5bGVBcHBsaWVkXCJcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJzdHlsZWVkaXRvclwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJzdHlsZWVkaXRvclwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIm5ld0RvY3VtZW50XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcIm5ld0RvY3VtZW50XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIm5ld1N0eWxlU2hlZXRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwibmV3U3R5bGVTaGVldFwiLFxuICAgICAgICAgICAgXCJ0ZXh0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic3R5bGVTaGVldFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm9sZC1zdHlsZXNoZWV0XCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7XG4gICAgICAgIFwiZG9jdW1lbnQtbG9hZFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwiZG9jdW1lbnRMb2FkXCIsXG4gICAgICAgICAgXCJzdHlsZVNoZWV0c1wiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImFycmF5Om9sZC1zdHlsZXNoZWV0XCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIH1cbiAgICB9LFxuICAgIFwiY29va2llb2JqZWN0XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiY29va2llb2JqZWN0XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwibmFtZVwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcInZhbHVlXCI6IFwibG9uZ3N0cmluZ1wiLFxuICAgICAgICBcInBhdGhcIjogXCJudWxsYWJsZTpzdHJpbmdcIixcbiAgICAgICAgXCJob3N0XCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwiaXNEb21haW5cIjogXCJib29sZWFuXCIsXG4gICAgICAgIFwiaXNTZWN1cmVcIjogXCJib29sZWFuXCIsXG4gICAgICAgIFwiaXNIdHRwT25seVwiOiBcImJvb2xlYW5cIixcbiAgICAgICAgXCJjcmVhdGlvblRpbWVcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJsYXN0QWNjZXNzZWRcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJleHBpcmVzXCI6IFwibnVtYmVyXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiY29va2llc3RvcmVvYmplY3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJjb29raWVzdG9yZW9iamVjdFwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcInRvdGFsXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwib2Zmc2V0XCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwiZGF0YVwiOiBcImFycmF5Om51bGxhYmxlOmNvb2tpZW9iamVjdFwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcInN0b3JhZ2VvYmplY3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJzdG9yYWdlb2JqZWN0XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwibmFtZVwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcInZhbHVlXCI6IFwibG9uZ3N0cmluZ1wiXG4gICAgICB9XG4gICAgfSxcbiAgICBcInN0b3JhZ2VzdG9yZW9iamVjdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInN0b3JhZ2VzdG9yZW9iamVjdFwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcInRvdGFsXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwib2Zmc2V0XCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwiZGF0YVwiOiBcImFycmF5Om51bGxhYmxlOnN0b3JhZ2VvYmplY3RcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJpZGJvYmplY3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJpZGJvYmplY3RcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJuYW1lXCI6IFwibnVsbGFibGU6c3RyaW5nXCIsXG4gICAgICAgIFwiZGJcIjogXCJudWxsYWJsZTpzdHJpbmdcIixcbiAgICAgICAgXCJvYmplY3RTdG9yZVwiOiBcIm51bGxhYmxlOnN0cmluZ1wiLFxuICAgICAgICBcIm9yaWdpblwiOiBcIm51bGxhYmxlOnN0cmluZ1wiLFxuICAgICAgICBcInZlcnNpb25cIjogXCJudWxsYWJsZTpudW1iZXJcIixcbiAgICAgICAgXCJvYmplY3RTdG9yZXNcIjogXCJudWxsYWJsZTpudW1iZXJcIixcbiAgICAgICAgXCJrZXlQYXRoXCI6IFwibnVsbGFibGU6c3RyaW5nXCIsXG4gICAgICAgIFwiYXV0b0luY3JlbWVudFwiOiBcIm51bGxhYmxlOmJvb2xlYW5cIixcbiAgICAgICAgXCJpbmRleGVzXCI6IFwibnVsbGFibGU6c3RyaW5nXCIsXG4gICAgICAgIFwidmFsdWVcIjogXCJudWxsYWJsZTpsb25nc3RyaW5nXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiaWRic3RvcmVvYmplY3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJpZGJzdG9yZW9iamVjdFwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcInRvdGFsXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwib2Zmc2V0XCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwiZGF0YVwiOiBcImFycmF5Om51bGxhYmxlOmlkYm9iamVjdFwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcInN0b3JlVXBkYXRlT2JqZWN0XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwic3RvcmVVcGRhdGVPYmplY3RcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJjaGFuZ2VkXCI6IFwibnVsbGFibGU6anNvblwiLFxuICAgICAgICBcImRlbGV0ZWRcIjogXCJudWxsYWJsZTpqc29uXCIsXG4gICAgICAgIFwiYWRkZWRcIjogXCJudWxsYWJsZTpqc29uXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiY29va2llc1wiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJjb29raWVzXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0U3RvcmVPYmplY3RzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFN0b3JlT2JqZWN0c1wiLFxuICAgICAgICAgICAgXCJob3N0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJuYW1lc1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTphcnJheTpzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwib3B0aW9uc1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiY29va2llc3RvcmVvYmplY3RcIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcImxvY2FsU3RvcmFnZVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJsb2NhbFN0b3JhZ2VcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRTdG9yZU9iamVjdHNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0U3RvcmVPYmplY3RzXCIsXG4gICAgICAgICAgICBcImhvc3RcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm5hbWVzXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmFycmF5OnN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJvcHRpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDIsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzdG9yYWdlc3RvcmVvYmplY3RcIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcInNlc3Npb25TdG9yYWdlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInNlc3Npb25TdG9yYWdlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0U3RvcmVPYmplY3RzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFN0b3JlT2JqZWN0c1wiLFxuICAgICAgICAgICAgXCJob3N0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJuYW1lc1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTphcnJheTpzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwib3B0aW9uc1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic3RvcmFnZXN0b3Jlb2JqZWN0XCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJpbmRleGVkREJcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiaW5kZXhlZERCXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0U3RvcmVPYmplY3RzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFN0b3JlT2JqZWN0c1wiLFxuICAgICAgICAgICAgXCJob3N0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJuYW1lc1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTphcnJheTpzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwib3B0aW9uc1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiaWRic3RvcmVvYmplY3RcIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcInN0b3JlbGlzdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInN0b3JlbGlzdFwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcImNvb2tpZXNcIjogXCJjb29raWVzXCIsXG4gICAgICAgIFwibG9jYWxTdG9yYWdlXCI6IFwibG9jYWxTdG9yYWdlXCIsXG4gICAgICAgIFwic2Vzc2lvblN0b3JhZ2VcIjogXCJzZXNzaW9uU3RvcmFnZVwiLFxuICAgICAgICBcImluZGV4ZWREQlwiOiBcImluZGV4ZWREQlwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcInN0b3JhZ2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwic3RvcmFnZVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImxpc3RTdG9yZXNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwibGlzdFN0b3Jlc1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcInN0b3JlbGlzdFwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge1xuICAgICAgICBcInN0b3Jlcy11cGRhdGVcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcInN0b3Jlc1VwZGF0ZVwiLFxuICAgICAgICAgIFwiZGF0YVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0b3JlVXBkYXRlT2JqZWN0XCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwic3RvcmVzLWNsZWFyZWRcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcInN0b3Jlc0NsZWFyZWRcIixcbiAgICAgICAgICBcImRhdGFcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJqc29uXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwic3RvcmVzLXJlbG9hZGVkXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJzdG9yZXNSZWxhb2RlZFwiLFxuICAgICAgICAgIFwiZGF0YVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImpzb25cIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJnY2xpXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImdjbGlcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzcGVjc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzcGVjc1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImV4ZWN1dGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZXhlY3V0ZVwiLFxuICAgICAgICAgICAgXCJ0eXBlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzdGF0ZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdGF0ZVwiLFxuICAgICAgICAgICAgXCJ0eXBlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwic3RhcnRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVtYmVyXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInJhbmtcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVtYmVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwidHlwZXBhcnNlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInR5cGVwYXJzZVwiLFxuICAgICAgICAgICAgXCJ0eXBlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicGFyYW1cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwidHlwZWluY3JlbWVudFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ0eXBlaW5jcmVtZW50XCIsXG4gICAgICAgICAgICBcInR5cGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJwYXJhbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzdHJpbmdcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInR5cGVkZWNyZW1lbnRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwidHlwZWRlY3JlbWVudFwiLFxuICAgICAgICAgICAgXCJ0eXBlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicGFyYW1cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZWxlY3Rpb25pbmZvXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNlbGVjdGlvbmluZm9cIixcbiAgICAgICAgICAgIFwidHlwZWRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInBhcmFtXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJhY3Rpb25cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwibWVtb3J5XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcIm1lbW9yeVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIm1lYXN1cmVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwibWVhc3VyZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcImV2ZW50TG9vcExhZ1wiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJldmVudExvb3BMYWdcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzdGFydFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdGFydFwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic3VjY2Vzc1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bWJlclwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic3RvcFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdG9wXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJldmVudC1sb29wLWxhZ1wiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwiZXZlbnQtbG9vcC1sYWdcIixcbiAgICAgICAgICBcInRpbWVcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJudW1iZXJcIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJwcmVmZXJlbmNlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInByZWZlcmVuY2VcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRCb29sUHJlZlwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRCb29sUHJlZlwiLFxuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0Q2hhclByZWZcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0Q2hhclByZWZcIixcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0SW50UHJlZlwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRJbnRQcmVmXCIsXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudW1iZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldEFsbFByZWZzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldEFsbFByZWZzXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNldEJvb2xQcmVmXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNldEJvb2xQcmVmXCIsXG4gICAgICAgICAgICBcIm5hbWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZXRDaGFyUHJlZlwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXRDaGFyUHJlZlwiLFxuICAgICAgICAgICAgXCJuYW1lXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2V0SW50UHJlZlwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXRJbnRQcmVmXCIsXG4gICAgICAgICAgICBcIm5hbWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJjbGVhclVzZXJQcmVmXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImNsZWFyVXNlclByZWZcIixcbiAgICAgICAgICAgIFwibmFtZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiZGV2aWNlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImRldmljZVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldERlc2NyaXB0aW9uXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldERlc2NyaXB0aW9uXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFdhbGxwYXBlclwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRXYWxscGFwZXJcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibG9uZ3N0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2NyZWVuc2hvdFRvRGF0YVVSTFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzY3JlZW5zaG90VG9EYXRhVVJMXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImxvbmdzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFJhd1Blcm1pc3Npb25zVGFibGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0UmF3UGVybWlzc2lvbnNUYWJsZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH1cbiAgfSxcbiAgXCJmcm9tXCI6IFwicm9vdFwiXG59XG4iLCJcInVzZSBzdHJpY3RcIjtcblxudmFyIENsYXNzID0gcmVxdWlyZShcIi4vY2xhc3NcIikuQ2xhc3M7XG52YXIgdXRpbCA9IHJlcXVpcmUoXCIuL3V0aWxcIik7XG52YXIga2V5cyA9IHV0aWwua2V5cztcbnZhciB2YWx1ZXMgPSB1dGlsLnZhbHVlcztcbnZhciBwYWlycyA9IHV0aWwucGFpcnM7XG52YXIgcXVlcnkgPSB1dGlsLnF1ZXJ5O1xudmFyIGZpbmRQYXRoID0gdXRpbC5maW5kUGF0aDtcbnZhciBFdmVudFRhcmdldCA9IHJlcXVpcmUoXCIuL2V2ZW50XCIpLkV2ZW50VGFyZ2V0O1xuXG52YXIgVHlwZVN5c3RlbSA9IENsYXNzKHtcbiAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKGNsaWVudCkge1xuICAgIHZhciB0eXBlcyA9IE9iamVjdC5jcmVhdGUobnVsbCk7XG4gICAgdmFyIHNwZWNpZmljYXRpb24gPSBPYmplY3QuY3JlYXRlKG51bGwpO1xuXG4gICAgdGhpcy5zcGVjaWZpY2F0aW9uID0gc3BlY2lmaWNhdGlvbjtcbiAgICB0aGlzLnR5cGVzID0gdHlwZXM7XG5cbiAgICB2YXIgdHlwZUZvciA9IGZ1bmN0aW9uIHR5cGVGb3IodHlwZU5hbWUpIHtcbiAgICAgIHR5cGVOYW1lID0gdHlwZU5hbWUgfHwgXCJwcmltaXRpdmVcIjtcbiAgICAgIGlmICghdHlwZXNbdHlwZU5hbWVdKSB7XG4gICAgICAgIGRlZmluZVR5cGUodHlwZU5hbWUpO1xuICAgICAgfVxuXG4gICAgICByZXR1cm4gdHlwZXNbdHlwZU5hbWVdO1xuICAgIH07XG4gICAgdGhpcy50eXBlRm9yID0gdHlwZUZvcjtcblxuICAgIHZhciBkZWZpbmVUeXBlID0gZnVuY3Rpb24oZGVzY3JpcHRvcikge1xuICAgICAgdmFyIHR5cGUgPSB2b2lkKDApO1xuICAgICAgaWYgKHR5cGVvZihkZXNjcmlwdG9yKSA9PT0gXCJzdHJpbmdcIikge1xuICAgICAgICBpZiAoZGVzY3JpcHRvci5pbmRleE9mKFwiOlwiKSA+IDApXG4gICAgICAgICAgdHlwZSA9IG1ha2VDb21wb3VuZFR5cGUoZGVzY3JpcHRvcik7XG4gICAgICAgIGVsc2UgaWYgKGRlc2NyaXB0b3IuaW5kZXhPZihcIiNcIikgPiAwKVxuICAgICAgICAgIHR5cGUgPSBuZXcgQWN0b3JEZXRhaWwoZGVzY3JpcHRvcik7XG4gICAgICAgICAgZWxzZSBpZiAoc3BlY2lmaWNhdGlvbltkZXNjcmlwdG9yXSlcbiAgICAgICAgICAgIHR5cGUgPSBtYWtlQ2F0ZWdvcnlUeXBlKHNwZWNpZmljYXRpb25bZGVzY3JpcHRvcl0pO1xuICAgICAgfSBlbHNlIHtcbiAgICAgICAgdHlwZSA9IG1ha2VDYXRlZ29yeVR5cGUoZGVzY3JpcHRvcik7XG4gICAgICB9XG5cbiAgICAgIGlmICh0eXBlKVxuICAgICAgICB0eXBlc1t0eXBlLm5hbWVdID0gdHlwZTtcbiAgICAgIGVsc2VcbiAgICAgICAgdGhyb3cgVHlwZUVycm9yKFwiSW52YWxpZCB0eXBlOiBcIiArIGRlc2NyaXB0b3IpO1xuICAgIH07XG4gICAgdGhpcy5kZWZpbmVUeXBlID0gZGVmaW5lVHlwZTtcblxuXG4gICAgdmFyIG1ha2VDb21wb3VuZFR5cGUgPSBmdW5jdGlvbihuYW1lKSB7XG4gICAgICB2YXIgaW5kZXggPSBuYW1lLmluZGV4T2YoXCI6XCIpO1xuICAgICAgdmFyIGJhc2VUeXBlID0gbmFtZS5zbGljZSgwLCBpbmRleCk7XG4gICAgICB2YXIgc3ViVHlwZSA9IG5hbWUuc2xpY2UoaW5kZXggKyAxKTtcblxuICAgICAgcmV0dXJuIGJhc2VUeXBlID09PSBcImFycmF5XCIgPyBuZXcgQXJyYXlPZihzdWJUeXBlKSA6XG4gICAgICBiYXNlVHlwZSA9PT0gXCJudWxsYWJsZVwiID8gbmV3IE1heWJlKHN1YlR5cGUpIDpcbiAgICAgIG51bGw7XG4gICAgfTtcblxuICAgIHZhciBtYWtlQ2F0ZWdvcnlUeXBlID0gZnVuY3Rpb24oZGVzY3JpcHRvcikge1xuICAgICAgdmFyIGNhdGVnb3J5ID0gZGVzY3JpcHRvci5jYXRlZ29yeTtcbiAgICAgIHJldHVybiBjYXRlZ29yeSA9PT0gXCJkaWN0XCIgPyBuZXcgRGljdGlvbmFyeShkZXNjcmlwdG9yKSA6XG4gICAgICBjYXRlZ29yeSA9PT0gXCJhY3RvclwiID8gbmV3IEFjdG9yKGRlc2NyaXB0b3IpIDpcbiAgICAgIG51bGw7XG4gICAgfTtcblxuICAgIHZhciByZWFkID0gZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQsIHR5cGVOYW1lKSB7XG4gICAgICByZXR1cm4gdHlwZUZvcih0eXBlTmFtZSkucmVhZChpbnB1dCwgY29udGV4dCk7XG4gICAgfVxuICAgIHRoaXMucmVhZCA9IHJlYWQ7XG5cbiAgICB2YXIgd3JpdGUgPSBmdW5jdGlvbihpbnB1dCwgY29udGV4dCwgdHlwZU5hbWUpIHtcbiAgICAgIHJldHVybiB0eXBlRm9yKHR5cGVOYW1lKS53cml0ZShpbnB1dCk7XG4gICAgfTtcbiAgICB0aGlzLndyaXRlID0gd3JpdGU7XG5cblxuICAgIHZhciBUeXBlID0gQ2xhc3Moe1xuICAgICAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKCkge1xuICAgICAgfSxcbiAgICAgIGdldCBuYW1lKCkge1xuICAgICAgICByZXR1cm4gdGhpcy5jYXRlZ29yeSA/IHRoaXMuY2F0ZWdvcnkgKyBcIjpcIiArIHRoaXMudHlwZSA6XG4gICAgICAgIHRoaXMudHlwZTtcbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICB0aHJvdyBuZXcgVHlwZUVycm9yKFwiYFR5cGVgIHN1YmNsYXNzIG11c3QgaW1wbGVtZW50IGByZWFkYFwiKTtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgdGhyb3cgbmV3IFR5cGVFcnJvcihcImBUeXBlYCBzdWJjbGFzcyBtdXN0IGltcGxlbWVudCBgd3JpdGVgXCIpO1xuICAgICAgfVxuICAgIH0pO1xuXG4gICAgdmFyIFByaW1pdHZlID0gQ2xhc3Moe1xuICAgICAgZXh0ZW5kczogVHlwZSxcbiAgICAgIGNvbnN0dWN0b3I6IGZ1bmN0aW9uKHR5cGUpIHtcbiAgICAgICAgdGhpcy50eXBlID0gdHlwZTtcbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICByZXR1cm4gaW5wdXQ7XG4gICAgICB9LFxuICAgICAgd3JpdGU6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHJldHVybiBpbnB1dDtcbiAgICAgIH1cbiAgICB9KTtcblxuICAgIHZhciBNYXliZSA9IENsYXNzKHtcbiAgICAgIGV4dGVuZHM6IFR5cGUsXG4gICAgICBjYXRlZ29yeTogXCJudWxsYWJsZVwiLFxuICAgICAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKHR5cGUpIHtcbiAgICAgICAgdGhpcy50eXBlID0gdHlwZTtcbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICByZXR1cm4gaW5wdXQgPT09IG51bGwgPyBudWxsIDpcbiAgICAgICAgaW5wdXQgPT09IHZvaWQoMCkgPyB2b2lkKDApIDpcbiAgICAgICAgcmVhZChpbnB1dCwgY29udGV4dCwgdGhpcy50eXBlKTtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgcmV0dXJuIGlucHV0ID09PSBudWxsID8gbnVsbCA6XG4gICAgICAgIGlucHV0ID09PSB2b2lkKDApID8gdm9pZCgwKSA6XG4gICAgICAgIHdyaXRlKGlucHV0LCBjb250ZXh0LCB0aGlzLnR5cGUpO1xuICAgICAgfVxuICAgIH0pO1xuXG4gICAgdmFyIEFycmF5T2YgPSBDbGFzcyh7XG4gICAgICBleHRlbmRzOiBUeXBlLFxuICAgICAgY2F0ZWdvcnk6IFwiYXJyYXlcIixcbiAgICAgIGNvbnN0cnVjdG9yOiBmdW5jdGlvbih0eXBlKSB7XG4gICAgICAgIHRoaXMudHlwZSA9IHR5cGU7XG4gICAgICB9LFxuICAgICAgcmVhZDogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgdmFyIHR5cGUgPSB0aGlzLnR5cGU7XG4gICAgICAgIHJldHVybiBpbnB1dC5tYXAoZnVuY3Rpb24oJCkgeyByZXR1cm4gcmVhZCgkLCBjb250ZXh0LCB0eXBlKSB9KTtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgdmFyIHR5cGUgPSB0aGlzLnR5cGU7XG4gICAgICAgIHJldHVybiBpbnB1dC5tYXAoZnVuY3Rpb24oJCkgeyByZXR1cm4gd3JpdGUoJCwgY29udGV4dCwgdHlwZSkgfSk7XG4gICAgICB9XG4gICAgfSk7XG5cbiAgICB2YXIgbWFrZUZpZWxkID0gZnVuY3Rpb24gbWFrZUZpZWxkKG5hbWUsIHR5cGUpIHtcbiAgICAgIHJldHVybiB7XG4gICAgICAgIGVudW1lcmFibGU6IHRydWUsXG4gICAgICAgIGNvbmZpZ3VyYWJsZTogdHJ1ZSxcbiAgICAgICAgZ2V0OiBmdW5jdGlvbigpIHtcbiAgICAgICAgICBPYmplY3QuZGVmaW5lUHJvcGVydHkodGhpcywgbmFtZSwge1xuICAgICAgICAgICAgY29uZmlndXJhYmxlOiBmYWxzZSxcbiAgICAgICAgICAgIHZhbHVlOiByZWFkKHRoaXMuc3RhdGVbbmFtZV0sIHRoaXMuY29udGV4dCwgdHlwZSlcbiAgICAgICAgICB9KTtcbiAgICAgICAgICByZXR1cm4gdGhpc1tuYW1lXTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH07XG5cbiAgICB2YXIgbWFrZUZpZWxkcyA9IGZ1bmN0aW9uKGRlc2NyaXB0b3IpIHtcbiAgICAgIHJldHVybiBwYWlycyhkZXNjcmlwdG9yKS5yZWR1Y2UoZnVuY3Rpb24oZmllbGRzLCBwYWlyKSB7XG4gICAgICAgIHZhciBuYW1lID0gcGFpclswXSwgdHlwZSA9IHBhaXJbMV07XG4gICAgICAgIGZpZWxkc1tuYW1lXSA9IG1ha2VGaWVsZChuYW1lLCB0eXBlKTtcbiAgICAgICAgcmV0dXJuIGZpZWxkcztcbiAgICAgIH0sIHt9KTtcbiAgICB9XG5cbiAgICB2YXIgRGljdGlvbmFyeVR5cGUgPSBDbGFzcyh7fSk7XG5cbiAgICB2YXIgRGljdGlvbmFyeSA9IENsYXNzKHtcbiAgICAgIGV4dGVuZHM6IFR5cGUsXG4gICAgICBjYXRlZ29yeTogXCJkaWN0XCIsXG4gICAgICBnZXQgbmFtZSgpIHsgcmV0dXJuIHRoaXMudHlwZTsgfSxcbiAgICAgIGNvbnN0cnVjdG9yOiBmdW5jdGlvbihkZXNjcmlwdG9yKSB7XG4gICAgICAgIHRoaXMudHlwZSA9IGRlc2NyaXB0b3IudHlwZU5hbWU7XG4gICAgICAgIHRoaXMudHlwZXMgPSBkZXNjcmlwdG9yLnNwZWNpYWxpemF0aW9ucztcblxuICAgICAgICB2YXIgcHJvdG8gPSBPYmplY3QuZGVmaW5lUHJvcGVydGllcyh7XG4gICAgICAgICAgZXh0ZW5kczogRGljdGlvbmFyeVR5cGUsXG4gICAgICAgICAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKHN0YXRlLCBjb250ZXh0KSB7XG4gICAgICAgICAgICBPYmplY3QuZGVmaW5lUHJvcGVydGllcyh0aGlzLCB7XG4gICAgICAgICAgICAgIHN0YXRlOiB7XG4gICAgICAgICAgICAgICAgZW51bWVyYWJsZTogZmFsc2UsXG4gICAgICAgICAgICAgICAgd3JpdGFibGU6IHRydWUsXG4gICAgICAgICAgICAgICAgY29uZmlndXJhYmxlOiB0cnVlLFxuICAgICAgICAgICAgICAgIHZhbHVlOiBzdGF0ZVxuICAgICAgICAgICAgICB9LFxuICAgICAgICAgICAgICBjb250ZXh0OiB7XG4gICAgICAgICAgICAgICAgZW51bWVyYWJsZTogZmFsc2UsXG4gICAgICAgICAgICAgICAgd3JpdGFibGU6IGZhbHNlLFxuICAgICAgICAgICAgICAgIGNvbmZpZ3VyYWJsZTogdHJ1ZSxcbiAgICAgICAgICAgICAgICB2YWx1ZTogY29udGV4dFxuICAgICAgICAgICAgICB9XG4gICAgICAgICAgICB9KTtcbiAgICAgICAgICB9XG4gICAgICAgIH0sIG1ha2VGaWVsZHModGhpcy50eXBlcykpO1xuXG4gICAgICAgIHRoaXMuY2xhc3MgPSBuZXcgQ2xhc3MocHJvdG8pO1xuICAgICAgfSxcbiAgICAgIHJlYWQ6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHJldHVybiBuZXcgdGhpcy5jbGFzcyhpbnB1dCwgY29udGV4dCk7XG4gICAgICB9LFxuICAgICAgd3JpdGU6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHZhciBvdXRwdXQgPSB7fTtcbiAgICAgICAgZm9yICh2YXIga2V5IGluIGlucHV0KSB7XG4gICAgICAgICAgb3V0cHV0W2tleV0gPSB3cml0ZSh2YWx1ZSwgY29udGV4dCwgdHlwZXNba2V5XSk7XG4gICAgICAgIH1cbiAgICAgICAgcmV0dXJuIG91dHB1dDtcbiAgICAgIH1cbiAgICB9KTtcblxuICAgIHZhciBtYWtlTWV0aG9kcyA9IGZ1bmN0aW9uKGRlc2NyaXB0b3JzKSB7XG4gICAgICByZXR1cm4gZGVzY3JpcHRvcnMucmVkdWNlKGZ1bmN0aW9uKG1ldGhvZHMsIGRlc2NyaXB0b3IpIHtcbiAgICAgICAgbWV0aG9kc1tkZXNjcmlwdG9yLm5hbWVdID0ge1xuICAgICAgICAgIGVudW1lcmFibGU6IHRydWUsXG4gICAgICAgICAgY29uZmlndXJhYmxlOiB0cnVlLFxuICAgICAgICAgIHdyaXRhYmxlOiBmYWxzZSxcbiAgICAgICAgICB2YWx1ZTogbWFrZU1ldGhvZChkZXNjcmlwdG9yKVxuICAgICAgICB9O1xuICAgICAgICByZXR1cm4gbWV0aG9kcztcbiAgICAgIH0sIHt9KTtcbiAgICB9O1xuXG4gICAgdmFyIG1ha2VFdmVudHMgPSBmdW5jdGlvbihkZXNjcmlwdG9ycykge1xuICAgICAgcmV0dXJuIHBhaXJzKGRlc2NyaXB0b3JzKS5yZWR1Y2UoZnVuY3Rpb24oZXZlbnRzLCBwYWlyKSB7XG4gICAgICAgIHZhciBuYW1lID0gcGFpclswXSwgZGVzY3JpcHRvciA9IHBhaXJbMV07XG4gICAgICAgIHZhciBldmVudCA9IG5ldyBFdmVudChuYW1lLCBkZXNjcmlwdG9yKTtcbiAgICAgICAgZXZlbnRzW2V2ZW50LmV2ZW50VHlwZV0gPSBldmVudDtcbiAgICAgICAgcmV0dXJuIGV2ZW50cztcbiAgICAgIH0sIE9iamVjdC5jcmVhdGUobnVsbCkpO1xuICAgIH07XG5cbiAgICB2YXIgQWN0b3IgPSBDbGFzcyh7XG4gICAgICBleHRlbmRzOiBUeXBlLFxuICAgICAgY2F0ZWdvcnk6IFwiYWN0b3JcIixcbiAgICAgIGdldCBuYW1lKCkgeyByZXR1cm4gdGhpcy50eXBlOyB9LFxuICAgICAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKGRlc2NyaXB0b3IpIHtcbiAgICAgICAgdGhpcy50eXBlID0gZGVzY3JpcHRvci50eXBlTmFtZTtcblxuICAgICAgICB2YXIgZXZlbnRzID0gbWFrZUV2ZW50cyhkZXNjcmlwdG9yLmV2ZW50cyB8fCB7fSk7XG4gICAgICAgIHZhciBmaWVsZHMgPSBtYWtlRmllbGRzKGRlc2NyaXB0b3IuZmllbGRzIHx8IHt9KTtcbiAgICAgICAgdmFyIG1ldGhvZHMgPSBtYWtlTWV0aG9kcyhkZXNjcmlwdG9yLm1ldGhvZHMgfHwgW10pO1xuXG5cbiAgICAgICAgdmFyIHByb3RvID0ge1xuICAgICAgICAgIGV4dGVuZHM6IEZyb250LFxuICAgICAgICAgIGNvbnN0cnVjdG9yOiBmdW5jdGlvbigpIHtcbiAgICAgICAgICAgIEZyb250LmFwcGx5KHRoaXMsIGFyZ3VtZW50cyk7XG4gICAgICAgICAgfSxcbiAgICAgICAgICBldmVudHM6IGV2ZW50c1xuICAgICAgICB9O1xuICAgICAgICBPYmplY3QuZGVmaW5lUHJvcGVydGllcyhwcm90bywgZmllbGRzKTtcbiAgICAgICAgT2JqZWN0LmRlZmluZVByb3BlcnRpZXMocHJvdG8sIG1ldGhvZHMpO1xuXG4gICAgICAgIHRoaXMuY2xhc3MgPSBDbGFzcyhwcm90byk7XG4gICAgICB9LFxuICAgICAgcmVhZDogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQsIGRldGFpbCkge1xuICAgICAgICB2YXIgc3RhdGUgPSB0eXBlb2YoaW5wdXQpID09PSBcInN0cmluZ1wiID8geyBhY3RvcjogaW5wdXQgfSA6IGlucHV0O1xuXG4gICAgICAgIHZhciBhY3RvciA9IGNsaWVudC5nZXQoc3RhdGUuYWN0b3IpIHx8IG5ldyB0aGlzLmNsYXNzKHN0YXRlLCBjb250ZXh0KTtcbiAgICAgICAgYWN0b3IuZm9ybShzdGF0ZSwgZGV0YWlsLCBjb250ZXh0KTtcblxuICAgICAgICByZXR1cm4gYWN0b3I7XG4gICAgICB9LFxuICAgICAgd3JpdGU6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0LCBkZXRhaWwpIHtcbiAgICAgICAgcmV0dXJuIGlucHV0LmlkO1xuICAgICAgfVxuICAgIH0pO1xuICAgIGV4cG9ydHMuQWN0b3IgPSBBY3RvcjtcblxuXG4gICAgdmFyIEFjdG9yRGV0YWlsID0gQ2xhc3Moe1xuICAgICAgZXh0ZW5kczogQWN0b3IsXG4gICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24obmFtZSkge1xuICAgICAgICB2YXIgcGFydHMgPSBuYW1lLnNwbGl0KFwiI1wiKVxuICAgICAgICB0aGlzLmFjdG9yVHlwZSA9IHBhcnRzWzBdXG4gICAgICAgIHRoaXMuZGV0YWlsID0gcGFydHNbMV07XG4gICAgICB9LFxuICAgICAgcmVhZDogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgcmV0dXJuIHR5cGVGb3IodGhpcy5hY3RvclR5cGUpLnJlYWQoaW5wdXQsIGNvbnRleHQsIHRoaXMuZGV0YWlsKTtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgcmV0dXJuIHR5cGVGb3IodGhpcy5hY3RvclR5cGUpLndyaXRlKGlucHV0LCBjb250ZXh0LCB0aGlzLmRldGFpbCk7XG4gICAgICB9XG4gICAgfSk7XG4gICAgZXhwb3J0cy5BY3RvckRldGFpbCA9IEFjdG9yRGV0YWlsO1xuXG4gICAgdmFyIE1ldGhvZCA9IENsYXNzKHtcbiAgICAgIGV4dGVuZHM6IFR5cGUsXG4gICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oZGVzY3JpcHRvcikge1xuICAgICAgICB0aGlzLnR5cGUgPSBkZXNjcmlwdG9yLm5hbWU7XG4gICAgICAgIHRoaXMucGF0aCA9IGZpbmRQYXRoKGRlc2NyaXB0b3IucmVzcG9uc2UsIFwiX3JldHZhbFwiKTtcbiAgICAgICAgdGhpcy5yZXNwb25zZVR5cGUgPSB0aGlzLnBhdGggJiYgcXVlcnkoZGVzY3JpcHRvci5yZXNwb25zZSwgdGhpcy5wYXRoKS5fcmV0dmFsO1xuICAgICAgICB0aGlzLnJlcXVlc3RUeXBlID0gZGVzY3JpcHRvci5yZXF1ZXN0LnR5cGU7XG5cbiAgICAgICAgdmFyIHBhcmFtcyA9IFtdO1xuICAgICAgICBmb3IgKHZhciBrZXkgaW4gZGVzY3JpcHRvci5yZXF1ZXN0KSB7XG4gICAgICAgICAgaWYgKGtleSAhPT0gXCJ0eXBlXCIpIHtcbiAgICAgICAgICAgIHZhciBwYXJhbSA9IGRlc2NyaXB0b3IucmVxdWVzdFtrZXldO1xuICAgICAgICAgICAgdmFyIGluZGV4ID0gXCJfYXJnXCIgaW4gcGFyYW0gPyBwYXJhbS5fYXJnIDogcGFyYW0uX29wdGlvbjtcbiAgICAgICAgICAgIHZhciBpc1BhcmFtID0gcGFyYW0uX29wdGlvbiA9PT0gaW5kZXg7XG4gICAgICAgICAgICB2YXIgaXNBcmd1bWVudCA9IHBhcmFtLl9hcmcgPT09IGluZGV4O1xuICAgICAgICAgICAgcGFyYW1zW2luZGV4XSA9IHtcbiAgICAgICAgICAgICAgdHlwZTogcGFyYW0udHlwZSxcbiAgICAgICAgICAgICAga2V5OiBrZXksXG4gICAgICAgICAgICAgIGluZGV4OiBpbmRleCxcbiAgICAgICAgICAgICAgaXNQYXJhbTogaXNQYXJhbSxcbiAgICAgICAgICAgICAgaXNBcmd1bWVudDogaXNBcmd1bWVudFxuICAgICAgICAgICAgfTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgICAgdGhpcy5wYXJhbXMgPSBwYXJhbXM7XG4gICAgICB9LFxuICAgICAgcmVhZDogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgcmV0dXJuIHJlYWQocXVlcnkoaW5wdXQsIHRoaXMucGF0aCksIGNvbnRleHQsIHRoaXMucmVzcG9uc2VUeXBlKTtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgcmV0dXJuIHRoaXMucGFyYW1zLnJlZHVjZShmdW5jdGlvbihyZXN1bHQsIHBhcmFtKSB7XG4gICAgICAgICAgcmVzdWx0W3BhcmFtLmtleV0gPSB3cml0ZShpbnB1dFtwYXJhbS5pbmRleF0sIGNvbnRleHQsIHBhcmFtLnR5cGUpO1xuICAgICAgICAgIHJldHVybiByZXN1bHQ7XG4gICAgICAgIH0sIHt0eXBlOiB0aGlzLnR5cGV9KTtcbiAgICAgIH1cbiAgICB9KTtcbiAgICBleHBvcnRzLk1ldGhvZCA9IE1ldGhvZDtcblxuICAgIHZhciBwcm9maWxlciA9IGZ1bmN0aW9uKG1ldGhvZCwgaWQpIHtcbiAgICAgIHJldHVybiBmdW5jdGlvbigpIHtcbiAgICAgICAgdmFyIHN0YXJ0ID0gbmV3IERhdGUoKTtcbiAgICAgICAgcmV0dXJuIG1ldGhvZC5hcHBseSh0aGlzLCBhcmd1bWVudHMpLnRoZW4oZnVuY3Rpb24ocmVzdWx0KSB7XG4gICAgICAgICAgdmFyIGVuZCA9IG5ldyBEYXRlKCk7XG4gICAgICAgICAgY2xpZW50LnRlbGVtZXRyeS5hZGQoaWQsICtlbmQgLSBzdGFydCk7XG4gICAgICAgICAgcmV0dXJuIHJlc3VsdDtcbiAgICAgICAgfSk7XG4gICAgICB9O1xuICAgIH07XG5cbiAgICB2YXIgZGVzdHJ1Y3RvciA9IGZ1bmN0aW9uKG1ldGhvZCkge1xuICAgICAgcmV0dXJuIGZ1bmN0aW9uKCkge1xuICAgICAgICByZXR1cm4gbWV0aG9kLmFwcGx5KHRoaXMsIGFyZ3VtZW50cykudGhlbihmdW5jdGlvbihyZXN1bHQpIHtcbiAgICAgICAgICBjbGllbnQucmVsZWFzZSh0aGlzKTtcbiAgICAgICAgICByZXR1cm4gcmVzdWx0O1xuICAgICAgICB9KTtcbiAgICAgIH07XG4gICAgfTtcblxuICAgIGZ1bmN0aW9uIG1ha2VNZXRob2QoZGVzY3JpcHRvcikge1xuICAgICAgdmFyIHR5cGUgPSBuZXcgTWV0aG9kKGRlc2NyaXB0b3IpO1xuICAgICAgdmFyIG1ldGhvZCA9IGRlc2NyaXB0b3Iub25ld2F5ID8gbWFrZVVuaWRpcmVjYXRpb25hbE1ldGhvZChkZXNjcmlwdG9yLCB0eXBlKSA6XG4gICAgICAgICAgICAgICAgICAgbWFrZUJpZGlyZWN0aW9uYWxNZXRob2QoZGVzY3JpcHRvciwgdHlwZSk7XG5cbiAgICAgIGlmIChkZXNjcmlwdG9yLnRlbGVtZXRyeSlcbiAgICAgICAgbWV0aG9kID0gcHJvZmlsZXIobWV0aG9kKTtcbiAgICAgIGlmIChkZXNjcmlwdG9yLnJlbGVhc2UpXG4gICAgICAgIG1ldGhvZCA9IGRlc3RydWN0b3IobWV0aG9kKTtcblxuICAgICAgcmV0dXJuIG1ldGhvZDtcbiAgICB9XG5cbiAgICB2YXIgbWFrZVVuaWRpcmVjYXRpb25hbE1ldGhvZCA9IGZ1bmN0aW9uKGRlc2NyaXB0b3IsIHR5cGUpIHtcbiAgICAgIHJldHVybiBmdW5jdGlvbigpIHtcbiAgICAgICAgdmFyIHBhY2tldCA9IHR5cGUud3JpdGUoYXJndW1lbnRzLCB0aGlzKTtcbiAgICAgICAgcGFja2V0LnRvID0gdGhpcy5pZDtcbiAgICAgICAgY2xpZW50LnNlbmQocGFja2V0KTtcbiAgICAgICAgcmV0dXJuIFByb21pc2UucmVzb2x2ZSh2b2lkKDApKTtcbiAgICAgIH07XG4gICAgfTtcblxuICAgIHZhciBtYWtlQmlkaXJlY3Rpb25hbE1ldGhvZCA9IGZ1bmN0aW9uKGRlc2NyaXB0b3IsIHR5cGUpIHtcbiAgICAgIHJldHVybiBmdW5jdGlvbigpIHtcbiAgICAgICAgdmFyIGNvbnRleHQgPSB0aGlzLmNvbnRleHQ7XG4gICAgICAgIHZhciBwYWNrZXQgPSB0eXBlLndyaXRlKGFyZ3VtZW50cywgY29udGV4dCk7XG4gICAgICAgIHZhciBjb250ZXh0ID0gdGhpcy5jb250ZXh0O1xuICAgICAgICBwYWNrZXQudG8gPSB0aGlzLmlkO1xuICAgICAgICByZXR1cm4gY2xpZW50LnJlcXVlc3QocGFja2V0KS50aGVuKGZ1bmN0aW9uKHBhY2tldCkge1xuICAgICAgICAgIHJldHVybiB0eXBlLnJlYWQocGFja2V0LCBjb250ZXh0KTtcbiAgICAgICAgfSk7XG4gICAgICB9O1xuICAgIH07XG5cbiAgICB2YXIgRXZlbnQgPSBDbGFzcyh7XG4gICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24obmFtZSwgZGVzY3JpcHRvcikge1xuICAgICAgICB0aGlzLm5hbWUgPSBkZXNjcmlwdG9yLnR5cGUgfHwgbmFtZTtcbiAgICAgICAgdGhpcy5ldmVudFR5cGUgPSBkZXNjcmlwdG9yLnR5cGUgfHwgbmFtZTtcbiAgICAgICAgdGhpcy50eXBlcyA9IE9iamVjdC5jcmVhdGUobnVsbCk7XG5cbiAgICAgICAgdmFyIHR5cGVzID0gdGhpcy50eXBlcztcbiAgICAgICAgZm9yICh2YXIga2V5IGluIGRlc2NyaXB0b3IpIHtcbiAgICAgICAgICBpZiAoa2V5ID09PSBcInR5cGVcIikge1xuICAgICAgICAgICAgdHlwZXNba2V5XSA9IFwic3RyaW5nXCI7XG4gICAgICAgICAgfSBlbHNlIHtcbiAgICAgICAgICAgIHR5cGVzW2tleV0gPSBkZXNjcmlwdG9yW2tleV0udHlwZTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICB2YXIgb3V0cHV0ID0ge307XG4gICAgICAgIHZhciB0eXBlcyA9IHRoaXMudHlwZXM7XG4gICAgICAgIGZvciAodmFyIGtleSBpbiBpbnB1dCkge1xuICAgICAgICAgIG91dHB1dFtrZXldID0gcmVhZChpbnB1dFtrZXldLCBjb250ZXh0LCB0eXBlc1trZXldKTtcbiAgICAgICAgfVxuICAgICAgICByZXR1cm4gb3V0cHV0O1xuICAgICAgfSxcbiAgICAgIHdyaXRlOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICB2YXIgb3V0cHV0ID0ge307XG4gICAgICAgIHZhciB0eXBlcyA9IHRoaXMudHlwZXM7XG4gICAgICAgIGZvciAodmFyIGtleSBpbiB0aGlzLnR5cGVzKSB7XG4gICAgICAgICAgb3V0cHV0W2tleV0gPSB3cml0ZShpbnB1dFtrZXldLCBjb250ZXh0LCB0eXBlc1trZXldKTtcbiAgICAgICAgfVxuICAgICAgICByZXR1cm4gb3V0cHV0O1xuICAgICAgfVxuICAgIH0pO1xuXG4gICAgdmFyIEZyb250ID0gQ2xhc3Moe1xuICAgICAgZXh0ZW5kczogRXZlbnRUYXJnZXQsXG4gICAgICBFdmVudFRhcmdldDogRXZlbnRUYXJnZXQsXG4gICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oc3RhdGUpIHtcbiAgICAgICAgdGhpcy5FdmVudFRhcmdldCgpO1xuICAgICAgICBPYmplY3QuZGVmaW5lUHJvcGVydGllcyh0aGlzLCAge1xuICAgICAgICAgIHN0YXRlOiB7XG4gICAgICAgICAgICBlbnVtZXJhYmxlOiBmYWxzZSxcbiAgICAgICAgICAgIHdyaXRhYmxlOiB0cnVlLFxuICAgICAgICAgICAgY29uZmlndXJhYmxlOiB0cnVlLFxuICAgICAgICAgICAgdmFsdWU6IHN0YXRlXG4gICAgICAgICAgfVxuICAgICAgICB9KTtcblxuICAgICAgICBjbGllbnQucmVnaXN0ZXIodGhpcyk7XG4gICAgICB9LFxuICAgICAgZ2V0IGlkKCkge1xuICAgICAgICByZXR1cm4gdGhpcy5zdGF0ZS5hY3RvcjtcbiAgICAgIH0sXG4gICAgICBnZXQgY29udGV4dCgpIHtcbiAgICAgICAgcmV0dXJuIHRoaXM7XG4gICAgICB9LFxuICAgICAgZm9ybTogZnVuY3Rpb24oc3RhdGUsIGRldGFpbCwgY29udGV4dCkge1xuICAgICAgICBpZiAodGhpcy5zdGF0ZSAhPT0gc3RhdGUpIHtcbiAgICAgICAgICBpZiAoZGV0YWlsKSB7XG4gICAgICAgICAgICB0aGlzLnN0YXRlW2RldGFpbF0gPSBzdGF0ZVtkZXRhaWxdO1xuICAgICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgICBwYWlycyhzdGF0ZSkuZm9yRWFjaChmdW5jdGlvbihwYWlyKSB7XG4gICAgICAgICAgICAgIHZhciBrZXkgPSBwYWlyWzBdLCB2YWx1ZSA9IHBhaXJbMV07XG4gICAgICAgICAgICAgIHRoaXMuc3RhdGVba2V5XSA9IHZhbHVlO1xuICAgICAgICAgICAgfSwgdGhpcyk7XG4gICAgICAgICAgfVxuICAgICAgICB9XG5cbiAgICAgICAgaWYgKGNvbnRleHQpIHtcbiAgICAgICAgICBjbGllbnQuc3VwZXJ2aXNlKGNvbnRleHQsIHRoaXMpO1xuICAgICAgICB9XG4gICAgICB9LFxuICAgICAgcmVxdWVzdFR5cGVzOiBmdW5jdGlvbigpIHtcbiAgICAgICAgcmV0dXJuIGNsaWVudC5yZXF1ZXN0KHtcbiAgICAgICAgICB0bzogdGhpcy5pZCxcbiAgICAgICAgICB0eXBlOiBcInJlcXVlc3RUeXBlc1wiXG4gICAgICAgIH0pLnRoZW4oZnVuY3Rpb24ocGFja2V0KSB7XG4gICAgICAgICAgcmV0dXJuIHBhY2tldC5yZXF1ZXN0VHlwZXM7XG4gICAgICAgIH0pO1xuICAgICAgfVxuICAgIH0pO1xuICAgIHR5cGVzLnByaW1pdGl2ZSA9IG5ldyBQcmltaXR2ZShcInByaW1pdGl2ZVwiKTtcbiAgICB0eXBlcy5zdHJpbmcgPSBuZXcgUHJpbWl0dmUoXCJzdHJpbmdcIik7XG4gICAgdHlwZXMubnVtYmVyID0gbmV3IFByaW1pdHZlKFwibnVtYmVyXCIpO1xuICAgIHR5cGVzLmJvb2xlYW4gPSBuZXcgUHJpbWl0dmUoXCJib29sZWFuXCIpO1xuICAgIHR5cGVzLmpzb24gPSBuZXcgUHJpbWl0dmUoXCJqc29uXCIpO1xuICAgIHR5cGVzLmFycmF5ID0gbmV3IFByaW1pdHZlKFwiYXJyYXlcIik7XG4gIH0sXG4gIHJlZ2lzdGVyVHlwZXM6IGZ1bmN0aW9uKGRlc2NyaXB0b3IpIHtcbiAgICB2YXIgc3BlY2lmaWNhdGlvbiA9IHRoaXMuc3BlY2lmaWNhdGlvbjtcbiAgICB2YWx1ZXMoZGVzY3JpcHRvci50eXBlcykuZm9yRWFjaChmdW5jdGlvbihkZXNjcmlwdG9yKSB7XG4gICAgICBzcGVjaWZpY2F0aW9uW2Rlc2NyaXB0b3IudHlwZU5hbWVdID0gZGVzY3JpcHRvcjtcbiAgICB9KTtcbiAgfVxufSk7XG5leHBvcnRzLlR5cGVTeXN0ZW0gPSBUeXBlU3lzdGVtO1xuIiwiXCJ1c2Ugc3RyaWN0XCI7XG5cbnZhciBrZXlzID0gT2JqZWN0LmtleXM7XG5leHBvcnRzLmtleXMgPSBrZXlzO1xuXG4vLyBSZXR1cm5zIGFycmF5IG9mIHZhbHVlcyBmb3IgdGhlIGdpdmVuIG9iamVjdC5cbnZhciB2YWx1ZXMgPSBmdW5jdGlvbihvYmplY3QpIHtcbiAgcmV0dXJuIGtleXMob2JqZWN0KS5tYXAoZnVuY3Rpb24oa2V5KSB7XG4gICAgcmV0dXJuIG9iamVjdFtrZXldXG4gIH0pO1xufTtcbmV4cG9ydHMudmFsdWVzID0gdmFsdWVzO1xuXG4vLyBSZXR1cm5zIFtrZXksIHZhbHVlXSBwYWlycyBmb3IgdGhlIGdpdmVuIG9iamVjdC5cbnZhciBwYWlycyA9IGZ1bmN0aW9uKG9iamVjdCkge1xuICByZXR1cm4ga2V5cyhvYmplY3QpLm1hcChmdW5jdGlvbihrZXkpIHtcbiAgICByZXR1cm4gW2tleSwgb2JqZWN0W2tleV1dXG4gIH0pO1xufTtcbmV4cG9ydHMucGFpcnMgPSBwYWlycztcblxuXG4vLyBRdWVyaWVzIGFuIG9iamVjdCBmb3IgdGhlIGZpZWxkIG5lc3RlZCB3aXRoIGluIGl0LlxudmFyIHF1ZXJ5ID0gZnVuY3Rpb24ob2JqZWN0LCBwYXRoKSB7XG4gIHJldHVybiBwYXRoLnJlZHVjZShmdW5jdGlvbihvYmplY3QsIGVudHJ5KSB7XG4gICAgcmV0dXJuIG9iamVjdCAmJiBvYmplY3RbZW50cnldXG4gIH0sIG9iamVjdCk7XG59O1xuZXhwb3J0cy5xdWVyeSA9IHF1ZXJ5O1xuXG52YXIgaXNPYmplY3QgPSBmdW5jdGlvbih4KSB7XG4gIHJldHVybiB4ICYmIHR5cGVvZih4KSA9PT0gXCJvYmplY3RcIlxufVxuXG52YXIgZmluZFBhdGggPSBmdW5jdGlvbihvYmplY3QsIGtleSkge1xuICB2YXIgcGF0aCA9IHZvaWQoMCk7XG4gIGlmIChvYmplY3QgJiYgdHlwZW9mKG9iamVjdCkgPT09IFwib2JqZWN0XCIpIHtcbiAgICB2YXIgbmFtZXMgPSBrZXlzKG9iamVjdCk7XG4gICAgaWYgKG5hbWVzLmluZGV4T2Yoa2V5KSA+PSAwKSB7XG4gICAgICBwYXRoID0gW107XG4gICAgfSBlbHNlIHtcbiAgICAgIHZhciBpbmRleCA9IDA7XG4gICAgICB2YXIgY291bnQgPSBuYW1lcy5sZW5ndGg7XG4gICAgICB3aGlsZSAoaW5kZXggPCBjb3VudCAmJiAhcGF0aCl7XG4gICAgICAgIHZhciBoZWFkID0gbmFtZXNbaW5kZXhdO1xuICAgICAgICB2YXIgdGFpbCA9IGZpbmRQYXRoKG9iamVjdFtoZWFkXSwga2V5KTtcbiAgICAgICAgcGF0aCA9IHRhaWwgPyBbaGVhZF0uY29uY2F0KHRhaWwpIDogdGFpbDtcbiAgICAgICAgaW5kZXggPSBpbmRleCArIDFcbiAgICAgIH1cbiAgICB9XG4gIH1cbiAgcmV0dXJuIHBhdGg7XG59O1xuZXhwb3J0cy5maW5kUGF0aCA9IGZpbmRQYXRoO1xuIl19
(1)
});
