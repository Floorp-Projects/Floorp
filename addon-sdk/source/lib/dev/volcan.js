/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
var MessageEvent = _dereq_("./event").MessageEvent;

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
        var message = new MessageEvent(packet.type, {
          data: event.read(packet)
        });
        actor.dispatchEvent(message);
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
    this.unregister(actor);
  }
});
exports.Client = Client;

},{"./class":3,"./event":5,"./specification/core.json":23,"./specification/protocol.json":24,"./type-system":25,"./util":26,"es6-promise":2}],5:[function(_dereq_,module,exports){
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
      if (typeof console.trace === 'function') {
        // not supported in IE 10
        console.trace();
      }
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
      },
      "methods": [
         {
          "name": "attach",
          "request": {},
          "response": { "_retval": "json" }
         }
      ],
      "events": {
        "tabNavigated": {
           "typeName": "tabNavigated"
        }
      }
    },
    "console": {
      "category": "actor",
      "typeName": "console",
      "methods": [
        {
          "name": "evaluateJS",
          "request": {
            "text": {
              "_option": 0,
              "type": "string"
            },
            "url": {
              "_option": 1,
              "type": "string"
            },
            "bindObjectActor": {
              "_option": 2,
              "type": "nullable:string"
            },
            "frameActor": {
              "_option": 2,
              "type": "nullable:string"
            },
            "selectedNodeActor": {
              "_option": 2,
              "type": "nullable:string"
            }
          },
          "response": {
            "_retval": "evaluatejsresponse"
          }
        }
      ],
      "events": {}
    },
    "evaluatejsresponse": {
      "category": "dict",
      "typeName": "evaluatejsresponse",
      "specializations": {
        "result": "object",
        "exception": "object",
        "exceptionMessage": "string",
        "input": "string"
      }
    },
    "object": {
      "category": "actor",
      "typeName": "object",
      "methods": [
         {
           "name": "property",
           "request": {
              "name": {
                "_arg": 0,
                "type": "string"
              }
           },
           "response": {
              "descriptor": {
                "_retval": "json"
              }
           }
         }
      ]
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
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZ2VuZXJhdGVkLmpzIiwic291cmNlcyI6WyIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvYnJvd3NlcmlmeS9ub2RlX21vZHVsZXMvYnJvd3Nlci1wYWNrL19wcmVsdWRlLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vYnJvd3Nlci9pbmRleC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL2Jyb3dzZXIvcHJvbWlzZS5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL2NsYXNzLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vY2xpZW50LmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vZXZlbnQuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvYnJvd3NlcmlmeS9ub2RlX21vZHVsZXMvZXZlbnRzL2V2ZW50cy5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL2luZGV4LmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvaXMtaW1wbGVtZW50ZWQuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZC9pbmRleC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL25vZGVfbW9kdWxlcy9lczUtZXh0L29iamVjdC9hc3NpZ24vaW5kZXguanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3QvYXNzaWduL2lzLWltcGxlbWVudGVkLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvb2JqZWN0L2Fzc2lnbi9zaGltLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvb2JqZWN0L2lzLWNhbGxhYmxlLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvb2JqZWN0L2tleXMvaW5kZXguanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3Qva2V5cy9pcy1pbXBsZW1lbnRlZC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL25vZGVfbW9kdWxlcy9lczUtZXh0L29iamVjdC9rZXlzL3NoaW0uanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3Qvbm9ybWFsaXplLW9wdGlvbnMuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9vYmplY3QvdmFsaWQtdmFsdWUuanMiLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9ub2RlX21vZHVsZXMvZXM2LXN5bWJvbC9ub2RlX21vZHVsZXMvZXM1LWV4dC9zdHJpbmcvIy9jb250YWlucy9pbmRleC5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL25vZGVfbW9kdWxlcy9lczUtZXh0L3N0cmluZy8jL2NvbnRhaW5zL2lzLWltcGxlbWVudGVkLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vbm9kZV9tb2R1bGVzL2VzNi1zeW1ib2wvbm9kZV9tb2R1bGVzL2VzNS1leHQvc3RyaW5nLyMvY29udGFpbnMvc2hpbS5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL25vZGVfbW9kdWxlcy9lczYtc3ltYm9sL3BvbHlmaWxsLmpzIiwiL1VzZXJzL2dvemFsYS9Qcm9qZWN0cy92b2xjYW4vc3BlY2lmaWNhdGlvbi9jb3JlLmpzb24iLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi9zcGVjaWZpY2F0aW9uL3Byb3RvY29sLmpzb24iLCIvVXNlcnMvZ296YWxhL1Byb2plY3RzL3ZvbGNhbi90eXBlLXN5c3RlbS5qcyIsIi9Vc2Vycy9nb3phbGEvUHJvamVjdHMvdm9sY2FuL3V0aWwuanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUE7QUNBQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNUQTtBQUNBO0FBQ0E7QUFDQTs7QUNIQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ3RCQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDaExBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDbkVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQy9TQTtBQUNBO0FBQ0E7QUFDQTs7QUNIQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNyQkE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDL0RBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNMQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNUQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ3RCQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDTEE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ0xBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNSQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ1BBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDdEJBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ05BO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNMQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDUkE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNQQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDckRBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQ3pKQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUMzdUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FDcmRBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBIiwic291cmNlc0NvbnRlbnQiOlsiKGZ1bmN0aW9uIGUodCxuLHIpe2Z1bmN0aW9uIHMobyx1KXtpZighbltvXSl7aWYoIXRbb10pe3ZhciBhPXR5cGVvZiByZXF1aXJlPT1cImZ1bmN0aW9uXCImJnJlcXVpcmU7aWYoIXUmJmEpcmV0dXJuIGEobywhMCk7aWYoaSlyZXR1cm4gaShvLCEwKTt0aHJvdyBuZXcgRXJyb3IoXCJDYW5ub3QgZmluZCBtb2R1bGUgJ1wiK28rXCInXCIpfXZhciBmPW5bb109e2V4cG9ydHM6e319O3Rbb11bMF0uY2FsbChmLmV4cG9ydHMsZnVuY3Rpb24oZSl7dmFyIG49dFtvXVsxXVtlXTtyZXR1cm4gcyhuP246ZSl9LGYsZi5leHBvcnRzLGUsdCxuLHIpfXJldHVybiBuW29dLmV4cG9ydHN9dmFyIGk9dHlwZW9mIHJlcXVpcmU9PVwiZnVuY3Rpb25cIiYmcmVxdWlyZTtmb3IodmFyIG89MDtvPHIubGVuZ3RoO28rKylzKHJbb10pO3JldHVybiBzfSkiLCJcInVzZSBzdHJpY3RcIjtcblxudmFyIENsaWVudCA9IHJlcXVpcmUoXCIuLi9jbGllbnRcIikuQ2xpZW50O1xuXG5mdW5jdGlvbiBjb25uZWN0KHBvcnQpIHtcbiAgdmFyIGNsaWVudCA9IG5ldyBDbGllbnQoKTtcbiAgcmV0dXJuIGNsaWVudC5jb25uZWN0KHBvcnQpO1xufVxuZXhwb3J0cy5jb25uZWN0ID0gY29ubmVjdDtcbiIsIlwidXNlIHN0cmljdFwiO1xuXG5leHBvcnRzLlByb21pc2UgPSBQcm9taXNlO1xuIiwiXCJ1c2Ugc3RyaWN0XCI7XG5cbnZhciBkZXNjcmliZSA9IE9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3I7XG52YXIgQ2xhc3MgPSBmdW5jdGlvbihmaWVsZHMpIHtcbiAgdmFyIG5hbWVzID0gT2JqZWN0LmtleXMoZmllbGRzKTtcbiAgdmFyIGNvbnN0cnVjdG9yID0gbmFtZXMuaW5kZXhPZihcImNvbnN0cnVjdG9yXCIpID49IDAgPyBmaWVsZHMuY29uc3RydWN0b3IgOlxuICAgICAgICAgICAgICAgICAgICBmdW5jdGlvbigpIHt9O1xuICB2YXIgYW5jZXN0b3IgPSBmaWVsZHMuZXh0ZW5kcyB8fCBPYmplY3Q7XG5cbiAgdmFyIGRlc2NyaXB0b3IgPSBuYW1lcy5yZWR1Y2UoZnVuY3Rpb24oZGVzY3JpcHRvciwga2V5KSB7XG4gICAgZGVzY3JpcHRvcltrZXldID0gZGVzY3JpYmUoZmllbGRzLCBrZXkpO1xuICAgIHJldHVybiBkZXNjcmlwdG9yO1xuICB9LCB7fSk7XG5cbiAgdmFyIHByb3RvdHlwZSA9IE9iamVjdC5jcmVhdGUoYW5jZXN0b3IucHJvdG90eXBlLCBkZXNjcmlwdG9yKTtcblxuICBjb25zdHJ1Y3Rvci5wcm90b3R5cGUgPSBwcm90b3R5cGU7XG4gIHByb3RvdHlwZS5jb25zdHJ1Y3RvciA9IGNvbnN0cnVjdG9yO1xuXG4gIHJldHVybiBjb25zdHJ1Y3Rvcjtcbn07XG5leHBvcnRzLkNsYXNzID0gQ2xhc3M7XG4iLCJcInVzZSBzdHJpY3RcIjtcblxudmFyIENsYXNzID0gcmVxdWlyZShcIi4vY2xhc3NcIikuQ2xhc3M7XG52YXIgVHlwZVN5c3RlbSA9IHJlcXVpcmUoXCIuL3R5cGUtc3lzdGVtXCIpLlR5cGVTeXN0ZW07XG52YXIgdmFsdWVzID0gcmVxdWlyZShcIi4vdXRpbFwiKS52YWx1ZXM7XG52YXIgUHJvbWlzZSA9IHJlcXVpcmUoXCJlczYtcHJvbWlzZVwiKS5Qcm9taXNlO1xudmFyIE1lc3NhZ2VFdmVudCA9IHJlcXVpcmUoXCIuL2V2ZW50XCIpLk1lc3NhZ2VFdmVudDtcblxudmFyIHNwZWNpZmljYXRpb24gPSByZXF1aXJlKFwiLi9zcGVjaWZpY2F0aW9uL2NvcmUuanNvblwiKTtcblxuZnVuY3Rpb24gcmVjb3ZlckFjdG9yRGVzY3JpcHRpb25zKGVycm9yKSB7XG4gIGNvbnNvbGUud2FybihcIkZhaWxlZCB0byBmZXRjaCBwcm90b2NvbCBzcGVjaWZpY2F0aW9uIChzZWUgcmVhc29uIGJlbG93KS4gXCIgK1xuICAgICAgICAgICAgICAgXCJVc2luZyBhIGZhbGxiYWNrIHByb3RvY2FsIHNwZWNpZmljYXRpb24hXCIsXG4gICAgICAgICAgICAgICBlcnJvcik7XG4gIHJldHVybiByZXF1aXJlKFwiLi9zcGVjaWZpY2F0aW9uL3Byb3RvY29sLmpzb25cIik7XG59XG5cbi8vIFR5cGUgdG8gcmVwcmVzZW50IHN1cGVydmlzZXIgYWN0b3IgcmVsYXRpb25zIHRvIGFjdG9ycyB0aGV5IHN1cGVydmlzZVxuLy8gaW4gdGVybXMgb2YgbGlmZXRpbWUgbWFuYWdlbWVudC5cbnZhciBTdXBlcnZpc29yID0gQ2xhc3Moe1xuICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oaWQpIHtcbiAgICB0aGlzLmlkID0gaWQ7XG4gICAgdGhpcy53b3JrZXJzID0gW107XG4gIH1cbn0pO1xuXG52YXIgVGVsZW1ldHJ5ID0gQ2xhc3Moe1xuICBhZGQ6IGZ1bmN0aW9uKGlkLCBtcykge1xuICAgIGNvbnNvbGUubG9nKFwidGVsZW1ldHJ5OjpcIiwgaWQsIG1zKVxuICB9XG59KTtcblxuLy8gQ29uc2lkZXIgbWFraW5nIGNsaWVudCBhIHJvb3QgYWN0b3IuXG5cbnZhciBDbGllbnQgPSBDbGFzcyh7XG4gIGNvbnN0cnVjdG9yOiBmdW5jdGlvbigpIHtcbiAgICB0aGlzLnJvb3QgPSBudWxsO1xuICAgIHRoaXMudGVsZW1ldHJ5ID0gbmV3IFRlbGVtZXRyeSgpO1xuXG4gICAgdGhpcy5zZXR1cENvbm5lY3Rpb24oKTtcbiAgICB0aGlzLnNldHVwTGlmZU1hbmFnZW1lbnQoKTtcbiAgICB0aGlzLnNldHVwVHlwZVN5c3RlbSgpO1xuICB9LFxuXG4gIHNldHVwQ29ubmVjdGlvbjogZnVuY3Rpb24oKSB7XG4gICAgdGhpcy5yZXF1ZXN0cyA9IFtdO1xuICB9LFxuICBzZXR1cExpZmVNYW5hZ2VtZW50OiBmdW5jdGlvbigpIHtcbiAgICB0aGlzLmNhY2hlID0gT2JqZWN0LmNyZWF0ZShudWxsKTtcbiAgICB0aGlzLmdyYXBoID0gT2JqZWN0LmNyZWF0ZShudWxsKTtcbiAgICB0aGlzLmdldCA9IHRoaXMuZ2V0LmJpbmQodGhpcyk7XG4gICAgdGhpcy5yZWxlYXNlID0gdGhpcy5yZWxlYXNlLmJpbmQodGhpcyk7XG4gIH0sXG4gIHNldHVwVHlwZVN5c3RlbTogZnVuY3Rpb24oKSB7XG4gICAgdGhpcy50eXBlU3lzdGVtID0gbmV3IFR5cGVTeXN0ZW0odGhpcyk7XG4gICAgdGhpcy50eXBlU3lzdGVtLnJlZ2lzdGVyVHlwZXMoc3BlY2lmaWNhdGlvbik7XG4gIH0sXG5cbiAgY29ubmVjdDogZnVuY3Rpb24ocG9ydCkge1xuICAgIHZhciBjbGllbnQgPSB0aGlzO1xuICAgIHJldHVybiBuZXcgUHJvbWlzZShmdW5jdGlvbihyZXNvbHZlLCByZWplY3QpIHtcbiAgICAgIGNsaWVudC5wb3J0ID0gcG9ydDtcbiAgICAgIHBvcnQub25tZXNzYWdlID0gY2xpZW50LnJlY2VpdmUuYmluZChjbGllbnQpO1xuICAgICAgY2xpZW50Lm9uUmVhZHkgPSByZXNvbHZlO1xuICAgICAgY2xpZW50Lm9uRmFpbCA9IHJlamVjdDtcblxuICAgICAgcG9ydC5zdGFydCgpO1xuICAgIH0pO1xuICB9LFxuICBzZW5kOiBmdW5jdGlvbihwYWNrZXQpIHtcbiAgICB0aGlzLnBvcnQucG9zdE1lc3NhZ2UocGFja2V0KTtcbiAgfSxcbiAgcmVxdWVzdDogZnVuY3Rpb24ocGFja2V0KSB7XG4gICAgdmFyIGNsaWVudCA9IHRoaXM7XG4gICAgcmV0dXJuIG5ldyBQcm9taXNlKGZ1bmN0aW9uKHJlc29sdmUsIHJlamVjdCkge1xuICAgICAgY2xpZW50LnJlcXVlc3RzLnB1c2gocGFja2V0LnRvLCB7IHJlc29sdmU6IHJlc29sdmUsIHJlamVjdDogcmVqZWN0IH0pO1xuICAgICAgY2xpZW50LnNlbmQocGFja2V0KTtcbiAgICB9KTtcbiAgfSxcblxuICByZWNlaXZlOiBmdW5jdGlvbihldmVudCkge1xuICAgIHZhciBwYWNrZXQgPSBldmVudC5kYXRhO1xuICAgIGlmICghdGhpcy5yb290KSB7XG4gICAgICBpZiAocGFja2V0LmZyb20gIT09IFwicm9vdFwiKVxuICAgICAgICB0aHJvdyBFcnJvcihcIkluaXRpYWwgcGFja2V0IG11c3QgYmUgZnJvbSByb290XCIpO1xuICAgICAgaWYgKCEoXCJhcHBsaWNhdGlvblR5cGVcIiBpbiBwYWNrZXQpKVxuICAgICAgICB0aHJvdyBFcnJvcihcIkluaXRpYWwgcGFja2V0IG11c3QgY29udGFpbiBhcHBsaWNhdGlvblR5cGUgZmllbGRcIik7XG5cbiAgICAgIHRoaXMucm9vdCA9IHRoaXMudHlwZVN5c3RlbS5yZWFkKFwicm9vdFwiLCBudWxsLCBcInJvb3RcIik7XG4gICAgICB0aGlzLnJvb3RcbiAgICAgICAgICAucHJvdG9jb2xEZXNjcmlwdGlvbigpXG4gICAgICAgICAgLmNhdGNoKHJlY292ZXJBY3RvckRlc2NyaXB0aW9ucylcbiAgICAgICAgICAudGhlbih0aGlzLnR5cGVTeXN0ZW0ucmVnaXN0ZXJUeXBlcy5iaW5kKHRoaXMudHlwZVN5c3RlbSkpXG4gICAgICAgICAgLnRoZW4odGhpcy5vblJlYWR5LmJpbmQodGhpcywgdGhpcy5yb290KSwgdGhpcy5vbkZhaWwpO1xuICAgIH0gZWxzZSB7XG4gICAgICB2YXIgYWN0b3IgPSB0aGlzLmdldChwYWNrZXQuZnJvbSkgfHwgdGhpcy5yb290O1xuICAgICAgdmFyIGV2ZW50ID0gYWN0b3IuZXZlbnRzW3BhY2tldC50eXBlXTtcbiAgICAgIGlmIChldmVudCkge1xuICAgICAgICB2YXIgbWVzc2FnZSA9IG5ldyBNZXNzYWdlRXZlbnQocGFja2V0LnR5cGUsIHtcbiAgICAgICAgICBkYXRhOiBldmVudC5yZWFkKHBhY2tldClcbiAgICAgICAgfSk7XG4gICAgICAgIGFjdG9yLmRpc3BhdGNoRXZlbnQobWVzc2FnZSk7XG4gICAgICB9IGVsc2Uge1xuICAgICAgICB2YXIgaW5kZXggPSB0aGlzLnJlcXVlc3RzLmluZGV4T2YoYWN0b3IuaWQpO1xuICAgICAgICBpZiAoaW5kZXggPj0gMCkge1xuICAgICAgICAgIHZhciByZXF1ZXN0ID0gdGhpcy5yZXF1ZXN0cy5zcGxpY2UoaW5kZXgsIDIpLnBvcCgpO1xuICAgICAgICAgIGlmIChwYWNrZXQuZXJyb3IpXG4gICAgICAgICAgICByZXF1ZXN0LnJlamVjdChwYWNrZXQpO1xuICAgICAgICAgIGVsc2VcbiAgICAgICAgICAgIHJlcXVlc3QucmVzb2x2ZShwYWNrZXQpO1xuICAgICAgICB9IGVsc2Uge1xuICAgICAgICAgIGNvbnNvbGUuZXJyb3IoRXJyb3IoXCJVbmV4cGVjdGVkIHBhY2tldCBcIiArIEpTT04uc3RyaW5naWZ5KHBhY2tldCwgMiwgMikpLFxuICAgICAgICAgICAgICAgICAgICAgICAgcGFja2V0LFxuICAgICAgICAgICAgICAgICAgICAgICAgdGhpcy5yZXF1ZXN0cy5zbGljZSgwKSk7XG4gICAgICAgIH1cbiAgICAgIH1cbiAgICB9XG4gIH0sXG5cbiAgZ2V0OiBmdW5jdGlvbihpZCkge1xuICAgIHJldHVybiB0aGlzLmNhY2hlW2lkXTtcbiAgfSxcbiAgc3VwZXJ2aXNvck9mOiBmdW5jdGlvbihhY3Rvcikge1xuICAgIGZvciAodmFyIGlkIGluIHRoaXMuZ3JhcGgpIHtcbiAgICAgIGlmICh0aGlzLmdyYXBoW2lkXS5pbmRleE9mKGFjdG9yLmlkKSA+PSAwKSB7XG4gICAgICAgIHJldHVybiBpZDtcbiAgICAgIH1cbiAgICB9XG4gIH0sXG4gIHdvcmtlcnNPZjogZnVuY3Rpb24oYWN0b3IpIHtcbiAgICByZXR1cm4gdGhpcy5ncmFwaFthY3Rvci5pZF07XG4gIH0sXG4gIHN1cGVydmlzZTogZnVuY3Rpb24oYWN0b3IsIHdvcmtlcikge1xuICAgIHZhciB3b3JrZXJzID0gdGhpcy53b3JrZXJzT2YoYWN0b3IpXG4gICAgaWYgKHdvcmtlcnMuaW5kZXhPZih3b3JrZXIuaWQpIDwgMCkge1xuICAgICAgd29ya2Vycy5wdXNoKHdvcmtlci5pZCk7XG4gICAgfVxuICB9LFxuICB1bnN1cGVydmlzZTogZnVuY3Rpb24oYWN0b3IsIHdvcmtlcikge1xuICAgIHZhciB3b3JrZXJzID0gdGhpcy53b3JrZXJzT2YoYWN0b3IpO1xuICAgIHZhciBpbmRleCA9IHdvcmtlcnMuaW5kZXhPZih3b3JrZXIuaWQpXG4gICAgaWYgKGluZGV4ID49IDApIHtcbiAgICAgIHdvcmtlcnMuc3BsaWNlKGluZGV4LCAxKVxuICAgIH1cbiAgfSxcblxuICByZWdpc3RlcjogZnVuY3Rpb24oYWN0b3IpIHtcbiAgICB2YXIgcmVnaXN0ZXJlZCA9IHRoaXMuZ2V0KGFjdG9yLmlkKTtcbiAgICBpZiAoIXJlZ2lzdGVyZWQpIHtcbiAgICAgIHRoaXMuY2FjaGVbYWN0b3IuaWRdID0gYWN0b3I7XG4gICAgICB0aGlzLmdyYXBoW2FjdG9yLmlkXSA9IFtdO1xuICAgIH0gZWxzZSBpZiAocmVnaXN0ZXJlZCAhPT0gYWN0b3IpIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcihcIkRpZmZlcmVudCBhY3RvciB3aXRoIHNhbWUgaWQgaXMgYWxyZWFkeSByZWdpc3RlcmVkXCIpO1xuICAgIH1cbiAgfSxcbiAgdW5yZWdpc3RlcjogZnVuY3Rpb24oYWN0b3IpIHtcbiAgICBpZiAodGhpcy5nZXQoYWN0b3IuaWQpKSB7XG4gICAgICBkZWxldGUgdGhpcy5jYWNoZVthY3Rvci5pZF07XG4gICAgICBkZWxldGUgdGhpcy5ncmFwaFthY3Rvci5pZF07XG4gICAgfVxuICB9LFxuXG4gIHJlbGVhc2U6IGZ1bmN0aW9uKGFjdG9yKSB7XG4gICAgdmFyIHN1cGVydmlzb3IgPSB0aGlzLnN1cGVydmlzb3JPZihhY3Rvcik7XG4gICAgaWYgKHN1cGVydmlzb3IpXG4gICAgICB0aGlzLnVuc3VwZXJ2aXNlKHN1cGVydmlzb3IsIGFjdG9yKTtcblxuICAgIHZhciB3b3JrZXJzID0gdGhpcy53b3JrZXJzT2YoYWN0b3IpXG5cbiAgICBpZiAod29ya2Vycykge1xuICAgICAgd29ya2Vycy5tYXAodGhpcy5nZXQpLmZvckVhY2godGhpcy5yZWxlYXNlKVxuICAgIH1cbiAgICB0aGlzLnVucmVnaXN0ZXIoYWN0b3IpO1xuICB9XG59KTtcbmV4cG9ydHMuQ2xpZW50ID0gQ2xpZW50O1xuIiwiXCJ1c2Ugc3RyaWN0XCI7XG5cbnZhciBTeW1ib2wgPSByZXF1aXJlKFwiZXM2LXN5bWJvbFwiKVxudmFyIEV2ZW50RW1pdHRlciA9IHJlcXVpcmUoXCJldmVudHNcIikuRXZlbnRFbWl0dGVyO1xudmFyIENsYXNzID0gcmVxdWlyZShcIi4vY2xhc3NcIikuQ2xhc3M7XG5cbnZhciAkYm91bmQgPSBTeW1ib2woXCJFdmVudFRhcmdldC9oYW5kbGVFdmVudFwiKTtcbnZhciAkZW1pdHRlciA9IFN5bWJvbChcIkV2ZW50VGFyZ2V0L2VtaXR0ZXJcIik7XG5cbmZ1bmN0aW9uIG1ha2VIYW5kbGVyKGhhbmRsZXIpIHtcbiAgcmV0dXJuIGZ1bmN0aW9uKGV2ZW50KSB7XG4gICAgaGFuZGxlci5oYW5kbGVFdmVudChldmVudCk7XG4gIH1cbn1cblxudmFyIEV2ZW50VGFyZ2V0ID0gQ2xhc3Moe1xuICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oKSB7XG4gICAgT2JqZWN0LmRlZmluZVByb3BlcnR5KHRoaXMsICRlbWl0dGVyLCB7XG4gICAgICBlbnVtZXJhYmxlOiBmYWxzZSxcbiAgICAgIGNvbmZpZ3VyYWJsZTogdHJ1ZSxcbiAgICAgIHdyaXRhYmxlOiB0cnVlLFxuICAgICAgdmFsdWU6IG5ldyBFdmVudEVtaXR0ZXIoKVxuICAgIH0pO1xuICB9LFxuICBhZGRFdmVudExpc3RlbmVyOiBmdW5jdGlvbih0eXBlLCBoYW5kbGVyKSB7XG4gICAgaWYgKHR5cGVvZihoYW5kbGVyKSA9PT0gXCJmdW5jdGlvblwiKSB7XG4gICAgICB0aGlzWyRlbWl0dGVyXS5vbih0eXBlLCBoYW5kbGVyKTtcbiAgICB9XG4gICAgZWxzZSBpZiAoaGFuZGxlciAmJiB0eXBlb2YoaGFuZGxlcikgPT09IFwib2JqZWN0XCIpIHtcbiAgICAgIGlmICghaGFuZGxlclskYm91bmRdKSBoYW5kbGVyWyRib3VuZF0gPSBtYWtlSGFuZGxlcihoYW5kbGVyKTtcbiAgICAgIHRoaXNbJGVtaXR0ZXJdLm9uKHR5cGUsIGhhbmRsZXJbJGJvdW5kXSk7XG4gICAgfVxuICB9LFxuICByZW1vdmVFdmVudExpc3RlbmVyOiBmdW5jdGlvbih0eXBlLCBoYW5kbGVyKSB7XG4gICAgaWYgKHR5cGVvZihoYW5kbGVyKSA9PT0gXCJmdW5jdGlvblwiKVxuICAgICAgdGhpc1skZW1pdHRlcl0ucmVtb3ZlTGlzdGVuZXIodHlwZSwgaGFuZGxlcik7XG4gICAgZWxzZSBpZiAoaGFuZGxlciAmJiBoYW5kbGVyWyRib3VuZF0pXG4gICAgICB0aGlzWyRlbWl0dGVyXS5yZW1vdmVMaXN0ZW5lcih0eXBlLCBoYW5kbGVyWyRib3VuZF0pO1xuICB9LFxuICBkaXNwYXRjaEV2ZW50OiBmdW5jdGlvbihldmVudCkge1xuICAgIGV2ZW50LnRhcmdldCA9IHRoaXM7XG4gICAgdGhpc1skZW1pdHRlcl0uZW1pdChldmVudC50eXBlLCBldmVudCk7XG4gIH1cbn0pO1xuZXhwb3J0cy5FdmVudFRhcmdldCA9IEV2ZW50VGFyZ2V0O1xuXG52YXIgTWVzc2FnZUV2ZW50ID0gQ2xhc3Moe1xuICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24odHlwZSwgb3B0aW9ucykge1xuICAgIG9wdGlvbnMgPSBvcHRpb25zIHx8IHt9O1xuICAgIHRoaXMudHlwZSA9IHR5cGU7XG4gICAgdGhpcy5kYXRhID0gb3B0aW9ucy5kYXRhID09PSB2b2lkKDApID8gbnVsbCA6IG9wdGlvbnMuZGF0YTtcblxuICAgIHRoaXMubGFzdEV2ZW50SWQgPSBvcHRpb25zLmxhc3RFdmVudElkIHx8IFwiXCI7XG4gICAgdGhpcy5vcmlnaW4gPSBvcHRpb25zLm9yaWdpbiB8fCBcIlwiO1xuICAgIHRoaXMuYnViYmxlcyA9IG9wdGlvbnMuYnViYmxlcyB8fCBmYWxzZTtcbiAgICB0aGlzLmNhbmNlbGFibGUgPSBvcHRpb25zLmNhbmNlbGFibGUgfHwgZmFsc2U7XG4gIH0sXG4gIHNvdXJjZTogbnVsbCxcbiAgcG9ydHM6IG51bGwsXG4gIHByZXZlbnREZWZhdWx0OiBmdW5jdGlvbigpIHtcbiAgfSxcbiAgc3RvcFByb3BhZ2F0aW9uOiBmdW5jdGlvbigpIHtcbiAgfSxcbiAgc3RvcEltbWVkaWF0ZVByb3BhZ2F0aW9uOiBmdW5jdGlvbigpIHtcbiAgfVxufSk7XG5leHBvcnRzLk1lc3NhZ2VFdmVudCA9IE1lc3NhZ2VFdmVudDtcbiIsIi8vIENvcHlyaWdodCBKb3llbnQsIEluYy4gYW5kIG90aGVyIE5vZGUgY29udHJpYnV0b3JzLlxuLy9cbi8vIFBlcm1pc3Npb24gaXMgaGVyZWJ5IGdyYW50ZWQsIGZyZWUgb2YgY2hhcmdlLCB0byBhbnkgcGVyc29uIG9idGFpbmluZyBhXG4vLyBjb3B5IG9mIHRoaXMgc29mdHdhcmUgYW5kIGFzc29jaWF0ZWQgZG9jdW1lbnRhdGlvbiBmaWxlcyAodGhlXG4vLyBcIlNvZnR3YXJlXCIpLCB0byBkZWFsIGluIHRoZSBTb2Z0d2FyZSB3aXRob3V0IHJlc3RyaWN0aW9uLCBpbmNsdWRpbmdcbi8vIHdpdGhvdXQgbGltaXRhdGlvbiB0aGUgcmlnaHRzIHRvIHVzZSwgY29weSwgbW9kaWZ5LCBtZXJnZSwgcHVibGlzaCxcbi8vIGRpc3RyaWJ1dGUsIHN1YmxpY2Vuc2UsIGFuZC9vciBzZWxsIGNvcGllcyBvZiB0aGUgU29mdHdhcmUsIGFuZCB0byBwZXJtaXRcbi8vIHBlcnNvbnMgdG8gd2hvbSB0aGUgU29mdHdhcmUgaXMgZnVybmlzaGVkIHRvIGRvIHNvLCBzdWJqZWN0IHRvIHRoZVxuLy8gZm9sbG93aW5nIGNvbmRpdGlvbnM6XG4vL1xuLy8gVGhlIGFib3ZlIGNvcHlyaWdodCBub3RpY2UgYW5kIHRoaXMgcGVybWlzc2lvbiBub3RpY2Ugc2hhbGwgYmUgaW5jbHVkZWRcbi8vIGluIGFsbCBjb3BpZXMgb3Igc3Vic3RhbnRpYWwgcG9ydGlvbnMgb2YgdGhlIFNvZnR3YXJlLlxuLy9cbi8vIFRIRSBTT0ZUV0FSRSBJUyBQUk9WSURFRCBcIkFTIElTXCIsIFdJVEhPVVQgV0FSUkFOVFkgT0YgQU5ZIEtJTkQsIEVYUFJFU1Ncbi8vIE9SIElNUExJRUQsIElOQ0xVRElORyBCVVQgTk9UIExJTUlURUQgVE8gVEhFIFdBUlJBTlRJRVMgT0Zcbi8vIE1FUkNIQU5UQUJJTElUWSwgRklUTkVTUyBGT1IgQSBQQVJUSUNVTEFSIFBVUlBPU0UgQU5EIE5PTklORlJJTkdFTUVOVC4gSU5cbi8vIE5PIEVWRU5UIFNIQUxMIFRIRSBBVVRIT1JTIE9SIENPUFlSSUdIVCBIT0xERVJTIEJFIExJQUJMRSBGT1IgQU5ZIENMQUlNLFxuLy8gREFNQUdFUyBPUiBPVEhFUiBMSUFCSUxJVFksIFdIRVRIRVIgSU4gQU4gQUNUSU9OIE9GIENPTlRSQUNULCBUT1JUIE9SXG4vLyBPVEhFUldJU0UsIEFSSVNJTkcgRlJPTSwgT1VUIE9GIE9SIElOIENPTk5FQ1RJT04gV0lUSCBUSEUgU09GVFdBUkUgT1IgVEhFXG4vLyBVU0UgT1IgT1RIRVIgREVBTElOR1MgSU4gVEhFIFNPRlRXQVJFLlxuXG5mdW5jdGlvbiBFdmVudEVtaXR0ZXIoKSB7XG4gIHRoaXMuX2V2ZW50cyA9IHRoaXMuX2V2ZW50cyB8fCB7fTtcbiAgdGhpcy5fbWF4TGlzdGVuZXJzID0gdGhpcy5fbWF4TGlzdGVuZXJzIHx8IHVuZGVmaW5lZDtcbn1cbm1vZHVsZS5leHBvcnRzID0gRXZlbnRFbWl0dGVyO1xuXG4vLyBCYWNrd2FyZHMtY29tcGF0IHdpdGggbm9kZSAwLjEwLnhcbkV2ZW50RW1pdHRlci5FdmVudEVtaXR0ZXIgPSBFdmVudEVtaXR0ZXI7XG5cbkV2ZW50RW1pdHRlci5wcm90b3R5cGUuX2V2ZW50cyA9IHVuZGVmaW5lZDtcbkV2ZW50RW1pdHRlci5wcm90b3R5cGUuX21heExpc3RlbmVycyA9IHVuZGVmaW5lZDtcblxuLy8gQnkgZGVmYXVsdCBFdmVudEVtaXR0ZXJzIHdpbGwgcHJpbnQgYSB3YXJuaW5nIGlmIG1vcmUgdGhhbiAxMCBsaXN0ZW5lcnMgYXJlXG4vLyBhZGRlZCB0byBpdC4gVGhpcyBpcyBhIHVzZWZ1bCBkZWZhdWx0IHdoaWNoIGhlbHBzIGZpbmRpbmcgbWVtb3J5IGxlYWtzLlxuRXZlbnRFbWl0dGVyLmRlZmF1bHRNYXhMaXN0ZW5lcnMgPSAxMDtcblxuLy8gT2J2aW91c2x5IG5vdCBhbGwgRW1pdHRlcnMgc2hvdWxkIGJlIGxpbWl0ZWQgdG8gMTAuIFRoaXMgZnVuY3Rpb24gYWxsb3dzXG4vLyB0aGF0IHRvIGJlIGluY3JlYXNlZC4gU2V0IHRvIHplcm8gZm9yIHVubGltaXRlZC5cbkV2ZW50RW1pdHRlci5wcm90b3R5cGUuc2V0TWF4TGlzdGVuZXJzID0gZnVuY3Rpb24obikge1xuICBpZiAoIWlzTnVtYmVyKG4pIHx8IG4gPCAwIHx8IGlzTmFOKG4pKVxuICAgIHRocm93IFR5cGVFcnJvcignbiBtdXN0IGJlIGEgcG9zaXRpdmUgbnVtYmVyJyk7XG4gIHRoaXMuX21heExpc3RlbmVycyA9IG47XG4gIHJldHVybiB0aGlzO1xufTtcblxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5lbWl0ID0gZnVuY3Rpb24odHlwZSkge1xuICB2YXIgZXIsIGhhbmRsZXIsIGxlbiwgYXJncywgaSwgbGlzdGVuZXJzO1xuXG4gIGlmICghdGhpcy5fZXZlbnRzKVxuICAgIHRoaXMuX2V2ZW50cyA9IHt9O1xuXG4gIC8vIElmIHRoZXJlIGlzIG5vICdlcnJvcicgZXZlbnQgbGlzdGVuZXIgdGhlbiB0aHJvdy5cbiAgaWYgKHR5cGUgPT09ICdlcnJvcicpIHtcbiAgICBpZiAoIXRoaXMuX2V2ZW50cy5lcnJvciB8fFxuICAgICAgICAoaXNPYmplY3QodGhpcy5fZXZlbnRzLmVycm9yKSAmJiAhdGhpcy5fZXZlbnRzLmVycm9yLmxlbmd0aCkpIHtcbiAgICAgIGVyID0gYXJndW1lbnRzWzFdO1xuICAgICAgaWYgKGVyIGluc3RhbmNlb2YgRXJyb3IpIHtcbiAgICAgICAgdGhyb3cgZXI7IC8vIFVuaGFuZGxlZCAnZXJyb3InIGV2ZW50XG4gICAgICB9IGVsc2Uge1xuICAgICAgICB0aHJvdyBUeXBlRXJyb3IoJ1VuY2F1Z2h0LCB1bnNwZWNpZmllZCBcImVycm9yXCIgZXZlbnQuJyk7XG4gICAgICB9XG4gICAgICByZXR1cm4gZmFsc2U7XG4gICAgfVxuICB9XG5cbiAgaGFuZGxlciA9IHRoaXMuX2V2ZW50c1t0eXBlXTtcblxuICBpZiAoaXNVbmRlZmluZWQoaGFuZGxlcikpXG4gICAgcmV0dXJuIGZhbHNlO1xuXG4gIGlmIChpc0Z1bmN0aW9uKGhhbmRsZXIpKSB7XG4gICAgc3dpdGNoIChhcmd1bWVudHMubGVuZ3RoKSB7XG4gICAgICAvLyBmYXN0IGNhc2VzXG4gICAgICBjYXNlIDE6XG4gICAgICAgIGhhbmRsZXIuY2FsbCh0aGlzKTtcbiAgICAgICAgYnJlYWs7XG4gICAgICBjYXNlIDI6XG4gICAgICAgIGhhbmRsZXIuY2FsbCh0aGlzLCBhcmd1bWVudHNbMV0pO1xuICAgICAgICBicmVhaztcbiAgICAgIGNhc2UgMzpcbiAgICAgICAgaGFuZGxlci5jYWxsKHRoaXMsIGFyZ3VtZW50c1sxXSwgYXJndW1lbnRzWzJdKTtcbiAgICAgICAgYnJlYWs7XG4gICAgICAvLyBzbG93ZXJcbiAgICAgIGRlZmF1bHQ6XG4gICAgICAgIGxlbiA9IGFyZ3VtZW50cy5sZW5ndGg7XG4gICAgICAgIGFyZ3MgPSBuZXcgQXJyYXkobGVuIC0gMSk7XG4gICAgICAgIGZvciAoaSA9IDE7IGkgPCBsZW47IGkrKylcbiAgICAgICAgICBhcmdzW2kgLSAxXSA9IGFyZ3VtZW50c1tpXTtcbiAgICAgICAgaGFuZGxlci5hcHBseSh0aGlzLCBhcmdzKTtcbiAgICB9XG4gIH0gZWxzZSBpZiAoaXNPYmplY3QoaGFuZGxlcikpIHtcbiAgICBsZW4gPSBhcmd1bWVudHMubGVuZ3RoO1xuICAgIGFyZ3MgPSBuZXcgQXJyYXkobGVuIC0gMSk7XG4gICAgZm9yIChpID0gMTsgaSA8IGxlbjsgaSsrKVxuICAgICAgYXJnc1tpIC0gMV0gPSBhcmd1bWVudHNbaV07XG5cbiAgICBsaXN0ZW5lcnMgPSBoYW5kbGVyLnNsaWNlKCk7XG4gICAgbGVuID0gbGlzdGVuZXJzLmxlbmd0aDtcbiAgICBmb3IgKGkgPSAwOyBpIDwgbGVuOyBpKyspXG4gICAgICBsaXN0ZW5lcnNbaV0uYXBwbHkodGhpcywgYXJncyk7XG4gIH1cblxuICByZXR1cm4gdHJ1ZTtcbn07XG5cbkV2ZW50RW1pdHRlci5wcm90b3R5cGUuYWRkTGlzdGVuZXIgPSBmdW5jdGlvbih0eXBlLCBsaXN0ZW5lcikge1xuICB2YXIgbTtcblxuICBpZiAoIWlzRnVuY3Rpb24obGlzdGVuZXIpKVxuICAgIHRocm93IFR5cGVFcnJvcignbGlzdGVuZXIgbXVzdCBiZSBhIGZ1bmN0aW9uJyk7XG5cbiAgaWYgKCF0aGlzLl9ldmVudHMpXG4gICAgdGhpcy5fZXZlbnRzID0ge307XG5cbiAgLy8gVG8gYXZvaWQgcmVjdXJzaW9uIGluIHRoZSBjYXNlIHRoYXQgdHlwZSA9PT0gXCJuZXdMaXN0ZW5lclwiISBCZWZvcmVcbiAgLy8gYWRkaW5nIGl0IHRvIHRoZSBsaXN0ZW5lcnMsIGZpcnN0IGVtaXQgXCJuZXdMaXN0ZW5lclwiLlxuICBpZiAodGhpcy5fZXZlbnRzLm5ld0xpc3RlbmVyKVxuICAgIHRoaXMuZW1pdCgnbmV3TGlzdGVuZXInLCB0eXBlLFxuICAgICAgICAgICAgICBpc0Z1bmN0aW9uKGxpc3RlbmVyLmxpc3RlbmVyKSA/XG4gICAgICAgICAgICAgIGxpc3RlbmVyLmxpc3RlbmVyIDogbGlzdGVuZXIpO1xuXG4gIGlmICghdGhpcy5fZXZlbnRzW3R5cGVdKVxuICAgIC8vIE9wdGltaXplIHRoZSBjYXNlIG9mIG9uZSBsaXN0ZW5lci4gRG9uJ3QgbmVlZCB0aGUgZXh0cmEgYXJyYXkgb2JqZWN0LlxuICAgIHRoaXMuX2V2ZW50c1t0eXBlXSA9IGxpc3RlbmVyO1xuICBlbHNlIGlmIChpc09iamVjdCh0aGlzLl9ldmVudHNbdHlwZV0pKVxuICAgIC8vIElmIHdlJ3ZlIGFscmVhZHkgZ290IGFuIGFycmF5LCBqdXN0IGFwcGVuZC5cbiAgICB0aGlzLl9ldmVudHNbdHlwZV0ucHVzaChsaXN0ZW5lcik7XG4gIGVsc2VcbiAgICAvLyBBZGRpbmcgdGhlIHNlY29uZCBlbGVtZW50LCBuZWVkIHRvIGNoYW5nZSB0byBhcnJheS5cbiAgICB0aGlzLl9ldmVudHNbdHlwZV0gPSBbdGhpcy5fZXZlbnRzW3R5cGVdLCBsaXN0ZW5lcl07XG5cbiAgLy8gQ2hlY2sgZm9yIGxpc3RlbmVyIGxlYWtcbiAgaWYgKGlzT2JqZWN0KHRoaXMuX2V2ZW50c1t0eXBlXSkgJiYgIXRoaXMuX2V2ZW50c1t0eXBlXS53YXJuZWQpIHtcbiAgICB2YXIgbTtcbiAgICBpZiAoIWlzVW5kZWZpbmVkKHRoaXMuX21heExpc3RlbmVycykpIHtcbiAgICAgIG0gPSB0aGlzLl9tYXhMaXN0ZW5lcnM7XG4gICAgfSBlbHNlIHtcbiAgICAgIG0gPSBFdmVudEVtaXR0ZXIuZGVmYXVsdE1heExpc3RlbmVycztcbiAgICB9XG5cbiAgICBpZiAobSAmJiBtID4gMCAmJiB0aGlzLl9ldmVudHNbdHlwZV0ubGVuZ3RoID4gbSkge1xuICAgICAgdGhpcy5fZXZlbnRzW3R5cGVdLndhcm5lZCA9IHRydWU7XG4gICAgICBjb25zb2xlLmVycm9yKCcobm9kZSkgd2FybmluZzogcG9zc2libGUgRXZlbnRFbWl0dGVyIG1lbW9yeSAnICtcbiAgICAgICAgICAgICAgICAgICAgJ2xlYWsgZGV0ZWN0ZWQuICVkIGxpc3RlbmVycyBhZGRlZC4gJyArXG4gICAgICAgICAgICAgICAgICAgICdVc2UgZW1pdHRlci5zZXRNYXhMaXN0ZW5lcnMoKSB0byBpbmNyZWFzZSBsaW1pdC4nLFxuICAgICAgICAgICAgICAgICAgICB0aGlzLl9ldmVudHNbdHlwZV0ubGVuZ3RoKTtcbiAgICAgIGlmICh0eXBlb2YgY29uc29sZS50cmFjZSA9PT0gJ2Z1bmN0aW9uJykge1xuICAgICAgICAvLyBub3Qgc3VwcG9ydGVkIGluIElFIDEwXG4gICAgICAgIGNvbnNvbGUudHJhY2UoKTtcbiAgICAgIH1cbiAgICB9XG4gIH1cblxuICByZXR1cm4gdGhpcztcbn07XG5cbkV2ZW50RW1pdHRlci5wcm90b3R5cGUub24gPSBFdmVudEVtaXR0ZXIucHJvdG90eXBlLmFkZExpc3RlbmVyO1xuXG5FdmVudEVtaXR0ZXIucHJvdG90eXBlLm9uY2UgPSBmdW5jdGlvbih0eXBlLCBsaXN0ZW5lcikge1xuICBpZiAoIWlzRnVuY3Rpb24obGlzdGVuZXIpKVxuICAgIHRocm93IFR5cGVFcnJvcignbGlzdGVuZXIgbXVzdCBiZSBhIGZ1bmN0aW9uJyk7XG5cbiAgdmFyIGZpcmVkID0gZmFsc2U7XG5cbiAgZnVuY3Rpb24gZygpIHtcbiAgICB0aGlzLnJlbW92ZUxpc3RlbmVyKHR5cGUsIGcpO1xuXG4gICAgaWYgKCFmaXJlZCkge1xuICAgICAgZmlyZWQgPSB0cnVlO1xuICAgICAgbGlzdGVuZXIuYXBwbHkodGhpcywgYXJndW1lbnRzKTtcbiAgICB9XG4gIH1cblxuICBnLmxpc3RlbmVyID0gbGlzdGVuZXI7XG4gIHRoaXMub24odHlwZSwgZyk7XG5cbiAgcmV0dXJuIHRoaXM7XG59O1xuXG4vLyBlbWl0cyBhICdyZW1vdmVMaXN0ZW5lcicgZXZlbnQgaWZmIHRoZSBsaXN0ZW5lciB3YXMgcmVtb3ZlZFxuRXZlbnRFbWl0dGVyLnByb3RvdHlwZS5yZW1vdmVMaXN0ZW5lciA9IGZ1bmN0aW9uKHR5cGUsIGxpc3RlbmVyKSB7XG4gIHZhciBsaXN0LCBwb3NpdGlvbiwgbGVuZ3RoLCBpO1xuXG4gIGlmICghaXNGdW5jdGlvbihsaXN0ZW5lcikpXG4gICAgdGhyb3cgVHlwZUVycm9yKCdsaXN0ZW5lciBtdXN0IGJlIGEgZnVuY3Rpb24nKTtcblxuICBpZiAoIXRoaXMuX2V2ZW50cyB8fCAhdGhpcy5fZXZlbnRzW3R5cGVdKVxuICAgIHJldHVybiB0aGlzO1xuXG4gIGxpc3QgPSB0aGlzLl9ldmVudHNbdHlwZV07XG4gIGxlbmd0aCA9IGxpc3QubGVuZ3RoO1xuICBwb3NpdGlvbiA9IC0xO1xuXG4gIGlmIChsaXN0ID09PSBsaXN0ZW5lciB8fFxuICAgICAgKGlzRnVuY3Rpb24obGlzdC5saXN0ZW5lcikgJiYgbGlzdC5saXN0ZW5lciA9PT0gbGlzdGVuZXIpKSB7XG4gICAgZGVsZXRlIHRoaXMuX2V2ZW50c1t0eXBlXTtcbiAgICBpZiAodGhpcy5fZXZlbnRzLnJlbW92ZUxpc3RlbmVyKVxuICAgICAgdGhpcy5lbWl0KCdyZW1vdmVMaXN0ZW5lcicsIHR5cGUsIGxpc3RlbmVyKTtcblxuICB9IGVsc2UgaWYgKGlzT2JqZWN0KGxpc3QpKSB7XG4gICAgZm9yIChpID0gbGVuZ3RoOyBpLS0gPiAwOykge1xuICAgICAgaWYgKGxpc3RbaV0gPT09IGxpc3RlbmVyIHx8XG4gICAgICAgICAgKGxpc3RbaV0ubGlzdGVuZXIgJiYgbGlzdFtpXS5saXN0ZW5lciA9PT0gbGlzdGVuZXIpKSB7XG4gICAgICAgIHBvc2l0aW9uID0gaTtcbiAgICAgICAgYnJlYWs7XG4gICAgICB9XG4gICAgfVxuXG4gICAgaWYgKHBvc2l0aW9uIDwgMClcbiAgICAgIHJldHVybiB0aGlzO1xuXG4gICAgaWYgKGxpc3QubGVuZ3RoID09PSAxKSB7XG4gICAgICBsaXN0Lmxlbmd0aCA9IDA7XG4gICAgICBkZWxldGUgdGhpcy5fZXZlbnRzW3R5cGVdO1xuICAgIH0gZWxzZSB7XG4gICAgICBsaXN0LnNwbGljZShwb3NpdGlvbiwgMSk7XG4gICAgfVxuXG4gICAgaWYgKHRoaXMuX2V2ZW50cy5yZW1vdmVMaXN0ZW5lcilcbiAgICAgIHRoaXMuZW1pdCgncmVtb3ZlTGlzdGVuZXInLCB0eXBlLCBsaXN0ZW5lcik7XG4gIH1cblxuICByZXR1cm4gdGhpcztcbn07XG5cbkV2ZW50RW1pdHRlci5wcm90b3R5cGUucmVtb3ZlQWxsTGlzdGVuZXJzID0gZnVuY3Rpb24odHlwZSkge1xuICB2YXIga2V5LCBsaXN0ZW5lcnM7XG5cbiAgaWYgKCF0aGlzLl9ldmVudHMpXG4gICAgcmV0dXJuIHRoaXM7XG5cbiAgLy8gbm90IGxpc3RlbmluZyBmb3IgcmVtb3ZlTGlzdGVuZXIsIG5vIG5lZWQgdG8gZW1pdFxuICBpZiAoIXRoaXMuX2V2ZW50cy5yZW1vdmVMaXN0ZW5lcikge1xuICAgIGlmIChhcmd1bWVudHMubGVuZ3RoID09PSAwKVxuICAgICAgdGhpcy5fZXZlbnRzID0ge307XG4gICAgZWxzZSBpZiAodGhpcy5fZXZlbnRzW3R5cGVdKVxuICAgICAgZGVsZXRlIHRoaXMuX2V2ZW50c1t0eXBlXTtcbiAgICByZXR1cm4gdGhpcztcbiAgfVxuXG4gIC8vIGVtaXQgcmVtb3ZlTGlzdGVuZXIgZm9yIGFsbCBsaXN0ZW5lcnMgb24gYWxsIGV2ZW50c1xuICBpZiAoYXJndW1lbnRzLmxlbmd0aCA9PT0gMCkge1xuICAgIGZvciAoa2V5IGluIHRoaXMuX2V2ZW50cykge1xuICAgICAgaWYgKGtleSA9PT0gJ3JlbW92ZUxpc3RlbmVyJykgY29udGludWU7XG4gICAgICB0aGlzLnJlbW92ZUFsbExpc3RlbmVycyhrZXkpO1xuICAgIH1cbiAgICB0aGlzLnJlbW92ZUFsbExpc3RlbmVycygncmVtb3ZlTGlzdGVuZXInKTtcbiAgICB0aGlzLl9ldmVudHMgPSB7fTtcbiAgICByZXR1cm4gdGhpcztcbiAgfVxuXG4gIGxpc3RlbmVycyA9IHRoaXMuX2V2ZW50c1t0eXBlXTtcblxuICBpZiAoaXNGdW5jdGlvbihsaXN0ZW5lcnMpKSB7XG4gICAgdGhpcy5yZW1vdmVMaXN0ZW5lcih0eXBlLCBsaXN0ZW5lcnMpO1xuICB9IGVsc2Uge1xuICAgIC8vIExJRk8gb3JkZXJcbiAgICB3aGlsZSAobGlzdGVuZXJzLmxlbmd0aClcbiAgICAgIHRoaXMucmVtb3ZlTGlzdGVuZXIodHlwZSwgbGlzdGVuZXJzW2xpc3RlbmVycy5sZW5ndGggLSAxXSk7XG4gIH1cbiAgZGVsZXRlIHRoaXMuX2V2ZW50c1t0eXBlXTtcblxuICByZXR1cm4gdGhpcztcbn07XG5cbkV2ZW50RW1pdHRlci5wcm90b3R5cGUubGlzdGVuZXJzID0gZnVuY3Rpb24odHlwZSkge1xuICB2YXIgcmV0O1xuICBpZiAoIXRoaXMuX2V2ZW50cyB8fCAhdGhpcy5fZXZlbnRzW3R5cGVdKVxuICAgIHJldCA9IFtdO1xuICBlbHNlIGlmIChpc0Z1bmN0aW9uKHRoaXMuX2V2ZW50c1t0eXBlXSkpXG4gICAgcmV0ID0gW3RoaXMuX2V2ZW50c1t0eXBlXV07XG4gIGVsc2VcbiAgICByZXQgPSB0aGlzLl9ldmVudHNbdHlwZV0uc2xpY2UoKTtcbiAgcmV0dXJuIHJldDtcbn07XG5cbkV2ZW50RW1pdHRlci5saXN0ZW5lckNvdW50ID0gZnVuY3Rpb24oZW1pdHRlciwgdHlwZSkge1xuICB2YXIgcmV0O1xuICBpZiAoIWVtaXR0ZXIuX2V2ZW50cyB8fCAhZW1pdHRlci5fZXZlbnRzW3R5cGVdKVxuICAgIHJldCA9IDA7XG4gIGVsc2UgaWYgKGlzRnVuY3Rpb24oZW1pdHRlci5fZXZlbnRzW3R5cGVdKSlcbiAgICByZXQgPSAxO1xuICBlbHNlXG4gICAgcmV0ID0gZW1pdHRlci5fZXZlbnRzW3R5cGVdLmxlbmd0aDtcbiAgcmV0dXJuIHJldDtcbn07XG5cbmZ1bmN0aW9uIGlzRnVuY3Rpb24oYXJnKSB7XG4gIHJldHVybiB0eXBlb2YgYXJnID09PSAnZnVuY3Rpb24nO1xufVxuXG5mdW5jdGlvbiBpc051bWJlcihhcmcpIHtcbiAgcmV0dXJuIHR5cGVvZiBhcmcgPT09ICdudW1iZXInO1xufVxuXG5mdW5jdGlvbiBpc09iamVjdChhcmcpIHtcbiAgcmV0dXJuIHR5cGVvZiBhcmcgPT09ICdvYmplY3QnICYmIGFyZyAhPT0gbnVsbDtcbn1cblxuZnVuY3Rpb24gaXNVbmRlZmluZWQoYXJnKSB7XG4gIHJldHVybiBhcmcgPT09IHZvaWQgMDtcbn1cbiIsIid1c2Ugc3RyaWN0JztcblxubW9kdWxlLmV4cG9ydHMgPSByZXF1aXJlKCcuL2lzLWltcGxlbWVudGVkJykoKSA/IFN5bWJvbCA6IHJlcXVpcmUoJy4vcG9seWZpbGwnKTtcbiIsIid1c2Ugc3RyaWN0JztcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAoKSB7XG5cdHZhciBzeW1ib2w7XG5cdGlmICh0eXBlb2YgU3ltYm9sICE9PSAnZnVuY3Rpb24nKSByZXR1cm4gZmFsc2U7XG5cdHN5bWJvbCA9IFN5bWJvbCgndGVzdCBzeW1ib2wnKTtcblx0dHJ5IHtcblx0XHRpZiAoU3RyaW5nKHN5bWJvbCkgIT09ICdTeW1ib2wgKHRlc3Qgc3ltYm9sKScpIHJldHVybiBmYWxzZTtcblx0fSBjYXRjaCAoZSkgeyByZXR1cm4gZmFsc2U7IH1cblx0aWYgKHR5cGVvZiBTeW1ib2wuaXRlcmF0b3IgPT09ICdzeW1ib2wnKSByZXR1cm4gdHJ1ZTtcblxuXHQvLyBSZXR1cm4gJ3RydWUnIGZvciBwb2x5ZmlsbHNcblx0aWYgKHR5cGVvZiBTeW1ib2wuaXNDb25jYXRTcHJlYWRhYmxlICE9PSAnb2JqZWN0JykgcmV0dXJuIGZhbHNlO1xuXHRpZiAodHlwZW9mIFN5bWJvbC5pc1JlZ0V4cCAhPT0gJ29iamVjdCcpIHJldHVybiBmYWxzZTtcblx0aWYgKHR5cGVvZiBTeW1ib2wuaXRlcmF0b3IgIT09ICdvYmplY3QnKSByZXR1cm4gZmFsc2U7XG5cdGlmICh0eXBlb2YgU3ltYm9sLnRvUHJpbWl0aXZlICE9PSAnb2JqZWN0JykgcmV0dXJuIGZhbHNlO1xuXHRpZiAodHlwZW9mIFN5bWJvbC50b1N0cmluZ1RhZyAhPT0gJ29iamVjdCcpIHJldHVybiBmYWxzZTtcblx0aWYgKHR5cGVvZiBTeW1ib2wudW5zY29wYWJsZXMgIT09ICdvYmplY3QnKSByZXR1cm4gZmFsc2U7XG5cblx0cmV0dXJuIHRydWU7XG59O1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG52YXIgYXNzaWduICAgICAgICA9IHJlcXVpcmUoJ2VzNS1leHQvb2JqZWN0L2Fzc2lnbicpXG4gICwgbm9ybWFsaXplT3B0cyA9IHJlcXVpcmUoJ2VzNS1leHQvb2JqZWN0L25vcm1hbGl6ZS1vcHRpb25zJylcbiAgLCBpc0NhbGxhYmxlICAgID0gcmVxdWlyZSgnZXM1LWV4dC9vYmplY3QvaXMtY2FsbGFibGUnKVxuICAsIGNvbnRhaW5zICAgICAgPSByZXF1aXJlKCdlczUtZXh0L3N0cmluZy8jL2NvbnRhaW5zJylcblxuICAsIGQ7XG5cbmQgPSBtb2R1bGUuZXhwb3J0cyA9IGZ1bmN0aW9uIChkc2NyLCB2YWx1ZS8qLCBvcHRpb25zKi8pIHtcblx0dmFyIGMsIGUsIHcsIG9wdGlvbnMsIGRlc2M7XG5cdGlmICgoYXJndW1lbnRzLmxlbmd0aCA8IDIpIHx8ICh0eXBlb2YgZHNjciAhPT0gJ3N0cmluZycpKSB7XG5cdFx0b3B0aW9ucyA9IHZhbHVlO1xuXHRcdHZhbHVlID0gZHNjcjtcblx0XHRkc2NyID0gbnVsbDtcblx0fSBlbHNlIHtcblx0XHRvcHRpb25zID0gYXJndW1lbnRzWzJdO1xuXHR9XG5cdGlmIChkc2NyID09IG51bGwpIHtcblx0XHRjID0gdyA9IHRydWU7XG5cdFx0ZSA9IGZhbHNlO1xuXHR9IGVsc2Uge1xuXHRcdGMgPSBjb250YWlucy5jYWxsKGRzY3IsICdjJyk7XG5cdFx0ZSA9IGNvbnRhaW5zLmNhbGwoZHNjciwgJ2UnKTtcblx0XHR3ID0gY29udGFpbnMuY2FsbChkc2NyLCAndycpO1xuXHR9XG5cblx0ZGVzYyA9IHsgdmFsdWU6IHZhbHVlLCBjb25maWd1cmFibGU6IGMsIGVudW1lcmFibGU6IGUsIHdyaXRhYmxlOiB3IH07XG5cdHJldHVybiAhb3B0aW9ucyA/IGRlc2MgOiBhc3NpZ24obm9ybWFsaXplT3B0cyhvcHRpb25zKSwgZGVzYyk7XG59O1xuXG5kLmdzID0gZnVuY3Rpb24gKGRzY3IsIGdldCwgc2V0LyosIG9wdGlvbnMqLykge1xuXHR2YXIgYywgZSwgb3B0aW9ucywgZGVzYztcblx0aWYgKHR5cGVvZiBkc2NyICE9PSAnc3RyaW5nJykge1xuXHRcdG9wdGlvbnMgPSBzZXQ7XG5cdFx0c2V0ID0gZ2V0O1xuXHRcdGdldCA9IGRzY3I7XG5cdFx0ZHNjciA9IG51bGw7XG5cdH0gZWxzZSB7XG5cdFx0b3B0aW9ucyA9IGFyZ3VtZW50c1szXTtcblx0fVxuXHRpZiAoZ2V0ID09IG51bGwpIHtcblx0XHRnZXQgPSB1bmRlZmluZWQ7XG5cdH0gZWxzZSBpZiAoIWlzQ2FsbGFibGUoZ2V0KSkge1xuXHRcdG9wdGlvbnMgPSBnZXQ7XG5cdFx0Z2V0ID0gc2V0ID0gdW5kZWZpbmVkO1xuXHR9IGVsc2UgaWYgKHNldCA9PSBudWxsKSB7XG5cdFx0c2V0ID0gdW5kZWZpbmVkO1xuXHR9IGVsc2UgaWYgKCFpc0NhbGxhYmxlKHNldCkpIHtcblx0XHRvcHRpb25zID0gc2V0O1xuXHRcdHNldCA9IHVuZGVmaW5lZDtcblx0fVxuXHRpZiAoZHNjciA9PSBudWxsKSB7XG5cdFx0YyA9IHRydWU7XG5cdFx0ZSA9IGZhbHNlO1xuXHR9IGVsc2Uge1xuXHRcdGMgPSBjb250YWlucy5jYWxsKGRzY3IsICdjJyk7XG5cdFx0ZSA9IGNvbnRhaW5zLmNhbGwoZHNjciwgJ2UnKTtcblx0fVxuXG5cdGRlc2MgPSB7IGdldDogZ2V0LCBzZXQ6IHNldCwgY29uZmlndXJhYmxlOiBjLCBlbnVtZXJhYmxlOiBlIH07XG5cdHJldHVybiAhb3B0aW9ucyA/IGRlc2MgOiBhc3NpZ24obm9ybWFsaXplT3B0cyhvcHRpb25zKSwgZGVzYyk7XG59O1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG5tb2R1bGUuZXhwb3J0cyA9IHJlcXVpcmUoJy4vaXMtaW1wbGVtZW50ZWQnKSgpXG5cdD8gT2JqZWN0LmFzc2lnblxuXHQ6IHJlcXVpcmUoJy4vc2hpbScpO1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG5tb2R1bGUuZXhwb3J0cyA9IGZ1bmN0aW9uICgpIHtcblx0dmFyIGFzc2lnbiA9IE9iamVjdC5hc3NpZ24sIG9iajtcblx0aWYgKHR5cGVvZiBhc3NpZ24gIT09ICdmdW5jdGlvbicpIHJldHVybiBmYWxzZTtcblx0b2JqID0geyBmb286ICdyYXonIH07XG5cdGFzc2lnbihvYmosIHsgYmFyOiAnZHdhJyB9LCB7IHRyenk6ICd0cnp5JyB9KTtcblx0cmV0dXJuIChvYmouZm9vICsgb2JqLmJhciArIG9iai50cnp5KSA9PT0gJ3JhemR3YXRyenknO1xufTtcbiIsIid1c2Ugc3RyaWN0JztcblxudmFyIGtleXMgID0gcmVxdWlyZSgnLi4va2V5cycpXG4gICwgdmFsdWUgPSByZXF1aXJlKCcuLi92YWxpZC12YWx1ZScpXG5cbiAgLCBtYXggPSBNYXRoLm1heDtcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAoZGVzdCwgc3JjLyosIOKApnNyY24qLykge1xuXHR2YXIgZXJyb3IsIGksIGwgPSBtYXgoYXJndW1lbnRzLmxlbmd0aCwgMiksIGFzc2lnbjtcblx0ZGVzdCA9IE9iamVjdCh2YWx1ZShkZXN0KSk7XG5cdGFzc2lnbiA9IGZ1bmN0aW9uIChrZXkpIHtcblx0XHR0cnkgeyBkZXN0W2tleV0gPSBzcmNba2V5XTsgfSBjYXRjaCAoZSkge1xuXHRcdFx0aWYgKCFlcnJvcikgZXJyb3IgPSBlO1xuXHRcdH1cblx0fTtcblx0Zm9yIChpID0gMTsgaSA8IGw7ICsraSkge1xuXHRcdHNyYyA9IGFyZ3VtZW50c1tpXTtcblx0XHRrZXlzKHNyYykuZm9yRWFjaChhc3NpZ24pO1xuXHR9XG5cdGlmIChlcnJvciAhPT0gdW5kZWZpbmVkKSB0aHJvdyBlcnJvcjtcblx0cmV0dXJuIGRlc3Q7XG59O1xuIiwiLy8gRGVwcmVjYXRlZFxuXG4ndXNlIHN0cmljdCc7XG5cbm1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKG9iaikgeyByZXR1cm4gdHlwZW9mIG9iaiA9PT0gJ2Z1bmN0aW9uJzsgfTtcbiIsIid1c2Ugc3RyaWN0JztcblxubW9kdWxlLmV4cG9ydHMgPSByZXF1aXJlKCcuL2lzLWltcGxlbWVudGVkJykoKVxuXHQ/IE9iamVjdC5rZXlzXG5cdDogcmVxdWlyZSgnLi9zaGltJyk7XG4iLCIndXNlIHN0cmljdCc7XG5cbm1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKCkge1xuXHR0cnkge1xuXHRcdE9iamVjdC5rZXlzKCdwcmltaXRpdmUnKTtcblx0XHRyZXR1cm4gdHJ1ZTtcblx0fSBjYXRjaCAoZSkgeyByZXR1cm4gZmFsc2U7IH1cbn07XG4iLCIndXNlIHN0cmljdCc7XG5cbnZhciBrZXlzID0gT2JqZWN0LmtleXM7XG5cbm1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKG9iamVjdCkge1xuXHRyZXR1cm4ga2V5cyhvYmplY3QgPT0gbnVsbCA/IG9iamVjdCA6IE9iamVjdChvYmplY3QpKTtcbn07XG4iLCIndXNlIHN0cmljdCc7XG5cbnZhciBhc3NpZ24gPSByZXF1aXJlKCcuL2Fzc2lnbicpXG5cbiAgLCBmb3JFYWNoID0gQXJyYXkucHJvdG90eXBlLmZvckVhY2hcbiAgLCBjcmVhdGUgPSBPYmplY3QuY3JlYXRlLCBnZXRQcm90b3R5cGVPZiA9IE9iamVjdC5nZXRQcm90b3R5cGVPZlxuXG4gICwgcHJvY2VzcztcblxucHJvY2VzcyA9IGZ1bmN0aW9uIChzcmMsIG9iaikge1xuXHR2YXIgcHJvdG8gPSBnZXRQcm90b3R5cGVPZihzcmMpO1xuXHRyZXR1cm4gYXNzaWduKHByb3RvID8gcHJvY2Vzcyhwcm90bywgb2JqKSA6IG9iaiwgc3JjKTtcbn07XG5cbm1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKG9wdGlvbnMvKiwg4oCmb3B0aW9ucyovKSB7XG5cdHZhciByZXN1bHQgPSBjcmVhdGUobnVsbCk7XG5cdGZvckVhY2guY2FsbChhcmd1bWVudHMsIGZ1bmN0aW9uIChvcHRpb25zKSB7XG5cdFx0aWYgKG9wdGlvbnMgPT0gbnVsbCkgcmV0dXJuO1xuXHRcdHByb2Nlc3MoT2JqZWN0KG9wdGlvbnMpLCByZXN1bHQpO1xuXHR9KTtcblx0cmV0dXJuIHJlc3VsdDtcbn07XG4iLCIndXNlIHN0cmljdCc7XG5cbm1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKHZhbHVlKSB7XG5cdGlmICh2YWx1ZSA9PSBudWxsKSB0aHJvdyBuZXcgVHlwZUVycm9yKFwiQ2Fubm90IHVzZSBudWxsIG9yIHVuZGVmaW5lZFwiKTtcblx0cmV0dXJuIHZhbHVlO1xufTtcbiIsIid1c2Ugc3RyaWN0JztcblxubW9kdWxlLmV4cG9ydHMgPSByZXF1aXJlKCcuL2lzLWltcGxlbWVudGVkJykoKVxuXHQ/IFN0cmluZy5wcm90b3R5cGUuY29udGFpbnNcblx0OiByZXF1aXJlKCcuL3NoaW0nKTtcbiIsIid1c2Ugc3RyaWN0JztcblxudmFyIHN0ciA9ICdyYXpkd2F0cnp5JztcblxubW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAoKSB7XG5cdGlmICh0eXBlb2Ygc3RyLmNvbnRhaW5zICE9PSAnZnVuY3Rpb24nKSByZXR1cm4gZmFsc2U7XG5cdHJldHVybiAoKHN0ci5jb250YWlucygnZHdhJykgPT09IHRydWUpICYmIChzdHIuY29udGFpbnMoJ2ZvbycpID09PSBmYWxzZSkpO1xufTtcbiIsIid1c2Ugc3RyaWN0JztcblxudmFyIGluZGV4T2YgPSBTdHJpbmcucHJvdG90eXBlLmluZGV4T2Y7XG5cbm1vZHVsZS5leHBvcnRzID0gZnVuY3Rpb24gKHNlYXJjaFN0cmluZy8qLCBwb3NpdGlvbiovKSB7XG5cdHJldHVybiBpbmRleE9mLmNhbGwodGhpcywgc2VhcmNoU3RyaW5nLCBhcmd1bWVudHNbMV0pID4gLTE7XG59O1xuIiwiJ3VzZSBzdHJpY3QnO1xuXG52YXIgZCA9IHJlcXVpcmUoJ2QnKVxuXG4gICwgY3JlYXRlID0gT2JqZWN0LmNyZWF0ZSwgZGVmaW5lUHJvcGVydGllcyA9IE9iamVjdC5kZWZpbmVQcm9wZXJ0aWVzXG4gICwgZ2VuZXJhdGVOYW1lLCBTeW1ib2w7XG5cbmdlbmVyYXRlTmFtZSA9IChmdW5jdGlvbiAoKSB7XG5cdHZhciBjcmVhdGVkID0gY3JlYXRlKG51bGwpO1xuXHRyZXR1cm4gZnVuY3Rpb24gKGRlc2MpIHtcblx0XHR2YXIgcG9zdGZpeCA9IDA7XG5cdFx0d2hpbGUgKGNyZWF0ZWRbZGVzYyArIChwb3N0Zml4IHx8ICcnKV0pICsrcG9zdGZpeDtcblx0XHRkZXNjICs9IChwb3N0Zml4IHx8ICcnKTtcblx0XHRjcmVhdGVkW2Rlc2NdID0gdHJ1ZTtcblx0XHRyZXR1cm4gJ0BAJyArIGRlc2M7XG5cdH07XG59KCkpO1xuXG5tb2R1bGUuZXhwb3J0cyA9IFN5bWJvbCA9IGZ1bmN0aW9uIChkZXNjcmlwdGlvbikge1xuXHR2YXIgc3ltYm9sO1xuXHRpZiAodGhpcyBpbnN0YW5jZW9mIFN5bWJvbCkge1xuXHRcdHRocm93IG5ldyBUeXBlRXJyb3IoJ1R5cGVFcnJvcjogU3ltYm9sIGlzIG5vdCBhIGNvbnN0cnVjdG9yJyk7XG5cdH1cblx0c3ltYm9sID0gY3JlYXRlKFN5bWJvbC5wcm90b3R5cGUpO1xuXHRkZXNjcmlwdGlvbiA9IChkZXNjcmlwdGlvbiA9PT0gdW5kZWZpbmVkID8gJycgOiBTdHJpbmcoZGVzY3JpcHRpb24pKTtcblx0cmV0dXJuIGRlZmluZVByb3BlcnRpZXMoc3ltYm9sLCB7XG5cdFx0X19kZXNjcmlwdGlvbl9fOiBkKCcnLCBkZXNjcmlwdGlvbiksXG5cdFx0X19uYW1lX186IGQoJycsIGdlbmVyYXRlTmFtZShkZXNjcmlwdGlvbikpXG5cdH0pO1xufTtcblxuT2JqZWN0LmRlZmluZVByb3BlcnRpZXMoU3ltYm9sLCB7XG5cdGNyZWF0ZTogZCgnJywgU3ltYm9sKCdjcmVhdGUnKSksXG5cdGhhc0luc3RhbmNlOiBkKCcnLCBTeW1ib2woJ2hhc0luc3RhbmNlJykpLFxuXHRpc0NvbmNhdFNwcmVhZGFibGU6IGQoJycsIFN5bWJvbCgnaXNDb25jYXRTcHJlYWRhYmxlJykpLFxuXHRpc1JlZ0V4cDogZCgnJywgU3ltYm9sKCdpc1JlZ0V4cCcpKSxcblx0aXRlcmF0b3I6IGQoJycsIFN5bWJvbCgnaXRlcmF0b3InKSksXG5cdHRvUHJpbWl0aXZlOiBkKCcnLCBTeW1ib2woJ3RvUHJpbWl0aXZlJykpLFxuXHR0b1N0cmluZ1RhZzogZCgnJywgU3ltYm9sKCd0b1N0cmluZ1RhZycpKSxcblx0dW5zY29wYWJsZXM6IGQoJycsIFN5bWJvbCgndW5zY29wYWJsZXMnKSlcbn0pO1xuXG5kZWZpbmVQcm9wZXJ0aWVzKFN5bWJvbC5wcm90b3R5cGUsIHtcblx0cHJvcGVyVG9TdHJpbmc6IGQoZnVuY3Rpb24gKCkge1xuXHRcdHJldHVybiAnU3ltYm9sICgnICsgdGhpcy5fX2Rlc2NyaXB0aW9uX18gKyAnKSc7XG5cdH0pLFxuXHR0b1N0cmluZzogZCgnJywgZnVuY3Rpb24gKCkgeyByZXR1cm4gdGhpcy5fX25hbWVfXzsgfSlcbn0pO1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KFN5bWJvbC5wcm90b3R5cGUsIFN5bWJvbC50b1ByaW1pdGl2ZSwgZCgnJyxcblx0ZnVuY3Rpb24gKGhpbnQpIHtcblx0XHR0aHJvdyBuZXcgVHlwZUVycm9yKFwiQ29udmVyc2lvbiBvZiBzeW1ib2wgb2JqZWN0cyBpcyBub3QgYWxsb3dlZFwiKTtcblx0fSkpO1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KFN5bWJvbC5wcm90b3R5cGUsIFN5bWJvbC50b1N0cmluZ1RhZywgZCgnYycsICdTeW1ib2wnKSk7XG4iLCJtb2R1bGUuZXhwb3J0cz17XG4gIFwidHlwZXNcIjoge1xuICAgIFwicm9vdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJyb290XCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZWNob1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInN0cmluZ1wiOiB7IFwiX2FyZ1wiOiAwLCBcInR5cGVcIjogXCJzdHJpbmdcIiB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic3RyaW5nXCI6IHsgXCJfcmV0dmFsXCI6IFwic3RyaW5nXCIgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImxpc3RUYWJzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHt9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjogeyBcIl9yZXR2YWxcIjogXCJ0YWJsaXN0XCIgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicHJvdG9jb2xEZXNjcmlwdGlvblwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7fSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHsgXCJfcmV0dmFsXCI6IFwianNvblwiIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJ0YWJMaXN0Q2hhbmdlZFwiOiB7fVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJ0YWJsaXN0XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwidGFibGlzdFwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcInNlbGVjdGVkXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwidGFic1wiOiBcImFycmF5OnRhYlwiLFxuICAgICAgICBcInVybFwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcImNvbnNvbGVBY3RvclwiOiBcImNvbnNvbGVcIixcbiAgICAgICAgXCJpbnNwZWN0b3JBY3RvclwiOiBcImluc3BlY3RvclwiLFxuICAgICAgICBcInN0eWxlU2hlZXRzQWN0b3JcIjogXCJzdHlsZXNoZWV0c1wiLFxuICAgICAgICBcInN0eWxlRWRpdG9yQWN0b3JcIjogXCJzdHlsZWVkaXRvclwiLFxuICAgICAgICBcIm1lbW9yeUFjdG9yXCI6IFwibWVtb3J5XCIsXG4gICAgICAgIFwiZXZlbnRMb29wTGFnQWN0b3JcIjogXCJldmVudExvb3BMYWdcIixcbiAgICAgICAgXCJwcmVmZXJlbmNlQWN0b3JcIjogXCJwcmVmZXJlbmNlXCIsXG4gICAgICAgIFwiZGV2aWNlQWN0b3JcIjogXCJkZXZpY2VcIixcblxuICAgICAgICBcInByb2ZpbGVyQWN0b3JcIjogXCJwcm9maWxlclwiLFxuICAgICAgICBcImNocm9tZURlYnVnZ2VyXCI6IFwiY2hyb21lRGVidWdnZXJcIixcbiAgICAgICAgXCJ3ZWJhcHBzQWN0b3JcIjogXCJ3ZWJhcHBzXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwidGFiXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInRhYlwiLFxuICAgICAgXCJmaWVsZHNcIjoge1xuICAgICAgICBcInRpdGxlXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwidXJsXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwib3V0ZXJXaW5kb3dJRFwiOiBcIm51bWJlclwiLFxuICAgICAgICBcImluc3BlY3RvckFjdG9yXCI6IFwiaW5zcGVjdG9yXCIsXG4gICAgICAgIFwiY2FsbFdhdGNoZXJBY3RvclwiOiBcImNhbGwtd2F0Y2hlclwiLFxuICAgICAgICBcImNhbnZhc0FjdG9yXCI6IFwiY2FudmFzXCIsXG4gICAgICAgIFwid2ViZ2xBY3RvclwiOiBcIndlYmdsXCIsXG4gICAgICAgIFwid2ViYXVkaW9BY3RvclwiOiBcIndlYmF1ZGlvXCIsXG4gICAgICAgIFwic3RvcmFnZUFjdG9yXCI6IFwic3RvcmFnZVwiLFxuICAgICAgICBcImdjbGlBY3RvclwiOiBcImdjbGlcIixcbiAgICAgICAgXCJtZW1vcnlBY3RvclwiOiBcIm1lbW9yeVwiLFxuICAgICAgICBcImV2ZW50TG9vcExhZ1wiOiBcImV2ZW50TG9vcExhZ1wiLFxuICAgICAgICBcInN0eWxlU2hlZXRzQWN0b3JcIjogXCJzdHlsZXNoZWV0c1wiLFxuICAgICAgICBcInN0eWxlRWRpdG9yQWN0b3JcIjogXCJzdHlsZWVkaXRvclwiLFxuXG4gICAgICAgIFwiY29uc29sZUFjdG9yXCI6IFwiY29uc29sZVwiLFxuICAgICAgICBcInRyYWNlQWN0b3JcIjogXCJ0cmFjZVwiXG4gICAgICB9LFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJhdHRhY2hcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge30sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7IFwiX3JldHZhbFwiOiBcImpzb25cIiB9XG4gICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge1xuICAgICAgICBcInRhYk5hdmlnYXRlZFwiOiB7XG4gICAgICAgICAgIFwidHlwZU5hbWVcIjogXCJ0YWJOYXZpZ2F0ZWRcIlxuICAgICAgICB9XG4gICAgICB9XG4gICAgfSxcbiAgICBcImNvbnNvbGVcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiY29uc29sZVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImV2YWx1YXRlSlNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0ZXh0XCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJ1cmxcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImJpbmRPYmplY3RBY3RvclwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwiZnJhbWVBY3RvclwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwic2VsZWN0ZWROb2RlQWN0b3JcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6c3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZXZhbHVhdGVqc3Jlc3BvbnNlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJldmFsdWF0ZWpzcmVzcG9uc2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJldmFsdWF0ZWpzcmVzcG9uc2VcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJyZXN1bHRcIjogXCJvYmplY3RcIixcbiAgICAgICAgXCJleGNlcHRpb25cIjogXCJvYmplY3RcIixcbiAgICAgICAgXCJleGNlcHRpb25NZXNzYWdlXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwiaW5wdXRcIjogXCJzdHJpbmdcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJvYmplY3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwib2JqZWN0XCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICAge1xuICAgICAgICAgICBcIm5hbWVcIjogXCJwcm9wZXJ0eVwiLFxuICAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgICBcIm5hbWVcIjoge1xuICAgICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICAgIH1cbiAgICAgICAgICAgfSxcbiAgICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICAgIFwiZGVzY3JpcHRvclwiOiB7XG4gICAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgICAgIH1cbiAgICAgICAgICAgfVxuICAgICAgICAgfVxuICAgICAgXVxuICAgIH1cbiAgfVxufVxuIiwibW9kdWxlLmV4cG9ydHM9e1xuICBcInR5cGVzXCI6IHtcbiAgICBcImxvbmdzdHJhY3RvclwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJsb25nc3RyYWN0b3JcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzdWJzdHJpbmdcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3Vic3RyaW5nXCIsXG4gICAgICAgICAgICBcInN0YXJ0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJlbmRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzdWJzdHJpbmdcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInJlbGVhc2VcIixcbiAgICAgICAgICBcInJlbGVhc2VcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicmVsZWFzZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJzdHlsZXNoZWV0XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInN0eWxlc2hlZXRcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ0b2dnbGVEaXNhYmxlZFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ0b2dnbGVEaXNhYmxlZFwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiZGlzYWJsZWRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRUZXh0XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFRleHRcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInRleHRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJsb25nc3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRPcmlnaW5hbFNvdXJjZXNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0T3JpZ2luYWxTb3VyY2VzXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJvcmlnaW5hbFNvdXJjZXNcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTphcnJheTpvcmlnaW5hbHNvdXJjZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0T3JpZ2luYWxMb2NhdGlvblwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRPcmlnaW5hbExvY2F0aW9uXCIsXG4gICAgICAgICAgICBcImxpbmVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVtYmVyXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImNvbHVtblwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudW1iZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJvcmlnaW5hbGxvY2F0aW9ucmVzcG9uc2VcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInVwZGF0ZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ1cGRhdGVcIixcbiAgICAgICAgICAgIFwidGV4dFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwidHJhbnNpdGlvblwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJwcm9wZXJ0eS1jaGFuZ2VcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcInByb3BlcnR5Q2hhbmdlXCIsXG4gICAgICAgICAgXCJwcm9wZXJ0eVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICBcInN0eWxlLWFwcGxpZWRcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcInN0eWxlQXBwbGllZFwiXG4gICAgICAgIH1cbiAgICAgIH1cbiAgICB9LFxuICAgIFwib3JpZ2luYWxzb3VyY2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwib3JpZ2luYWxzb3VyY2VcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRUZXh0XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFRleHRcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInRleHRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJsb25nc3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJzdHlsZXNoZWV0c1wiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJzdHlsZXNoZWV0c1wiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFN0eWxlU2hlZXRzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFN0eWxlU2hlZXRzXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzdHlsZVNoZWV0c1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImFycmF5OnN0eWxlc2hlZXRcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImFkZFN0eWxlU2hlZXRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYWRkU3R5bGVTaGVldFwiLFxuICAgICAgICAgICAgXCJ0ZXh0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwic3R5bGVTaGVldFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcInN0eWxlc2hlZXRcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcIm9yaWdpbmFsbG9jYXRpb25yZXNwb25zZVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcIm9yaWdpbmFsbG9jYXRpb25yZXNwb25zZVwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcInNvdXJjZVwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcImxpbmVcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJjb2x1bW5cIjogXCJudW1iZXJcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJkb21ub2RlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImRvbW5vZGVcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXROb2RlVmFsdWVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0Tm9kZVZhbHVlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImxvbmdzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNldE5vZGVWYWx1ZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXROb2RlVmFsdWVcIixcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldEltYWdlRGF0YVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRJbWFnZURhdGFcIixcbiAgICAgICAgICAgIFwibWF4RGltXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOm51bWJlclwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImltYWdlRGF0YVwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwibW9kaWZ5QXR0cmlidXRlc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJtb2RpZnlBdHRyaWJ1dGVzXCIsXG4gICAgICAgICAgICBcIm1vZGlmaWNhdGlvbnNcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYXJyYXk6anNvblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJhcHBsaWVkc3R5bGVcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJhcHBsaWVkc3R5bGVcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJydWxlXCI6IFwiZG9tc3R5bGVydWxlI2FjdG9yaWRcIixcbiAgICAgICAgXCJpbmhlcml0ZWRcIjogXCJudWxsYWJsZTpkb21ub2RlI2FjdG9yaWRcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJtYXRjaGVkc2VsZWN0b3JcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJtYXRjaGVkc2VsZWN0b3JcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJydWxlXCI6IFwiZG9tc3R5bGVydWxlI2FjdG9yaWRcIixcbiAgICAgICAgXCJzZWxlY3RvclwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcInZhbHVlXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwic3RhdHVzXCI6IFwibnVtYmVyXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwibWF0Y2hlZHNlbGVjdG9ycmVzcG9uc2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJtYXRjaGVkc2VsZWN0b3JyZXNwb25zZVwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcInJ1bGVzXCI6IFwiYXJyYXk6ZG9tc3R5bGVydWxlXCIsXG4gICAgICAgIFwic2hlZXRzXCI6IFwiYXJyYXk6c3R5bGVzaGVldFwiLFxuICAgICAgICBcIm1hdGNoZWRcIjogXCJhcnJheTptYXRjaGVkc2VsZWN0b3JcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJhcHBsaWVkU3R5bGVzUmV0dXJuXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiYXBwbGllZFN0eWxlc1JldHVyblwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcImVudHJpZXNcIjogXCJhcnJheTphcHBsaWVkc3R5bGVcIixcbiAgICAgICAgXCJydWxlc1wiOiBcImFycmF5OmRvbXN0eWxlcnVsZVwiLFxuICAgICAgICBcInNoZWV0c1wiOiBcImFycmF5OnN0eWxlc2hlZXRcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJwYWdlc3R5bGVcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwicGFnZXN0eWxlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0Q29tcHV0ZWRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0Q29tcHV0ZWRcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm1hcmtNYXRjaGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwib25seU1hdGNoZWRcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJmaWx0ZXJcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJjb21wdXRlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldE1hdGNoZWRTZWxlY3RvcnNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0TWF0Y2hlZFNlbGVjdG9yc1wiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicHJvcGVydHlcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImZpbHRlclwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJtYXRjaGVkc2VsZWN0b3JyZXNwb25zZVwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0QXBwbGllZFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRBcHBsaWVkXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJpbmhlcml0ZWRcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJtYXRjaGVkU2VsZWN0b3JzXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwiZmlsdGVyXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImFwcGxpZWRTdHlsZXNSZXR1cm5cIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldExheW91dFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRMYXlvdXRcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImF1dG9NYXJnaW5zXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJkb21zdHlsZXJ1bGVcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZG9tc3R5bGVydWxlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwibW9kaWZ5UHJvcGVydGllc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJtb2RpZnlQcm9wZXJ0aWVzXCIsXG4gICAgICAgICAgICBcIm1vZGlmaWNhdGlvbnNcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYXJyYXk6anNvblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwicnVsZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRvbXN0eWxlcnVsZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiaGlnaGxpZ2h0ZXJcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiaGlnaGxpZ2h0ZXJcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzaG93Qm94TW9kZWxcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2hvd0JveE1vZGVsXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJyZWdpb25cIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImhpZGVCb3hNb2RlbFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJoaWRlQm94TW9kZWxcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicGlja1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJwaWNrXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImNhbmNlbFBpY2tcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiY2FuY2VsUGlja1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJpbWFnZURhdGFcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJpbWFnZURhdGFcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJkYXRhXCI6IFwibnVsbGFibGU6bG9uZ3N0cmluZ1wiLFxuICAgICAgICBcInNpemVcIjogXCJqc29uXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiZGlzY29ubmVjdGVkTm9kZVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImRpc2Nvbm5lY3RlZE5vZGVcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJub2RlXCI6IFwiZG9tbm9kZVwiLFxuICAgICAgICBcIm5ld1BhcmVudHNcIjogXCJhcnJheTpkb21ub2RlXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiZGlzY29ubmVjdGVkTm9kZUFycmF5XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZGlzY29ubmVjdGVkTm9kZUFycmF5XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwibm9kZXNcIjogXCJhcnJheTpkb21ub2RlXCIsXG4gICAgICAgIFwibmV3UGFyZW50c1wiOiBcImFycmF5OmRvbW5vZGVcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJkb21tdXRhdGlvblwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImRvbW11dGF0aW9uXCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7fVxuICAgIH0sXG4gICAgXCJkb21ub2RlbGlzdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJkb21ub2RlbGlzdFwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIml0ZW1cIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaXRlbVwiLFxuICAgICAgICAgICAgXCJpdGVtXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRpc2Nvbm5lY3RlZE5vZGVcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIml0ZW1zXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcIml0ZW1zXCIsXG4gICAgICAgICAgICBcInN0YXJ0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOm51bWJlclwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJlbmRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6bnVtYmVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZGlzY29ubmVjdGVkTm9kZUFycmF5XCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJyZWxlYXNlXCIsXG4gICAgICAgICAgXCJyZWxlYXNlXCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlbGVhc2VcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiZG9tdHJhdmVyc2FsYXJyYXlcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJkb210cmF2ZXJzYWxhcnJheVwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcIm5vZGVzXCI6IFwiYXJyYXk6ZG9tbm9kZVwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcImRvbXdhbGtlclwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJkb213YWxrZXJcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJyZWxlYXNlXCIsXG4gICAgICAgICAgXCJyZWxlYXNlXCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlbGVhc2VcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicGlja1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJwaWNrXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZGlzY29ubmVjdGVkTm9kZVwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiY2FuY2VsUGlja1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJjYW5jZWxQaWNrXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImhpZ2hsaWdodFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJoaWdobGlnaHRcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImRvY3VtZW50XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImRvY3VtZW50XCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6ZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImRvY3VtZW50RWxlbWVudFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJkb2N1bWVudEVsZW1lbnRcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicGFyZW50c1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJwYXJlbnRzXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzYW1lRG9jdW1lbnRcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJub2Rlc1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImFycmF5OmRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInJldGFpbk5vZGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicmV0YWluTm9kZVwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwidW5yZXRhaW5Ob2RlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInVucmV0YWluTm9kZVwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicmVsZWFzZU5vZGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicmVsZWFzZU5vZGVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImZvcmNlXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJjaGlsZHJlblwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJjaGlsZHJlblwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwibWF4Tm9kZXNcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImNlbnRlclwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInN0YXJ0XCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwid2hhdFRvU2hvd1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJkb210cmF2ZXJzYWxhcnJheVwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2libGluZ3NcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2libGluZ3NcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm1heE5vZGVzXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJjZW50ZXJcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzdGFydFwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIndoYXRUb1Nob3dcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZG9tdHJhdmVyc2FsYXJyYXlcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIm5leHRTaWJsaW5nXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcIm5leHRTaWJsaW5nXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJ3aGF0VG9TaG93XCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bGxhYmxlOmRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInByZXZpb3VzU2libGluZ1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJwcmV2aW91c1NpYmxpbmdcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIndoYXRUb1Nob3dcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibnVsbGFibGU6ZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicXVlcnlTZWxlY3RvclwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJxdWVyeVNlbGVjdG9yXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzZWxlY3RvclwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJkaXNjb25uZWN0ZWROb2RlXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJxdWVyeVNlbGVjdG9yQWxsXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInF1ZXJ5U2VsZWN0b3JBbGxcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInNlbGVjdG9yXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwibGlzdFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImRvbW5vZGVsaXN0XCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRTdWdnZXN0aW9uc0ZvclF1ZXJ5XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFN1Z2dlc3Rpb25zRm9yUXVlcnlcIixcbiAgICAgICAgICAgIFwicXVlcnlcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImNvbXBsZXRpbmdcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInNlbGVjdG9yU3RhdGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJsaXN0XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYXJyYXk6YXJyYXk6c3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJhZGRQc2V1ZG9DbGFzc0xvY2tcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYWRkUHNldWRvQ2xhc3NMb2NrXCIsXG4gICAgICAgICAgICBcIm5vZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZG9tbm9kZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJwc2V1ZG9DbGFzc1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicGFyZW50c1wiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiaGlkZU5vZGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaGlkZU5vZGVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInVuaGlkZU5vZGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwidW5oaWRlTm9kZVwiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicmVtb3ZlUHNldWRvQ2xhc3NMb2NrXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlbW92ZVBzZXVkb0NsYXNzTG9ja1wiLFxuICAgICAgICAgICAgXCJub2RlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImRvbW5vZGVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicHNldWRvQ2xhc3NcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInBhcmVudHNcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImNsZWFyUHNldWRvQ2xhc3NMb2Nrc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJjbGVhclBzZXVkb0NsYXNzTG9ja3NcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudWxsYWJsZTpkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImlubmVySFRNTFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJpbm5lckhUTUxcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImxvbmdzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcIm91dGVySFRNTFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJvdXRlckhUTUxcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImxvbmdzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNldE91dGVySFRNTFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXRPdXRlckhUTUxcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJyZW1vdmVOb2RlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInJlbW92ZU5vZGVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJuZXh0U2libGluZ1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bGxhYmxlOmRvbW5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImluc2VydEJlZm9yZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJpbnNlcnRCZWZvcmVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInBhcmVudFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInNpYmxpbmdcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6ZG9tbm9kZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRNdXRhdGlvbnNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0TXV0YXRpb25zXCIsXG4gICAgICAgICAgICBcImNsZWFudXBcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJtdXRhdGlvbnNcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJhcnJheTpkb21tdXRhdGlvblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiaXNJbkRPTVRyZWVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaXNJbkRPTVRyZWVcIixcbiAgICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJkb21ub2RlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJhdHRhY2hlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldE5vZGVBY3RvckZyb21PYmplY3RBY3RvclwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXROb2RlQWN0b3JGcm9tT2JqZWN0QWN0b3JcIixcbiAgICAgICAgICAgIFwib2JqZWN0QWN0b3JJRFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIm5vZGVGcm9udFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bGxhYmxlOmRpc2Nvbm5lY3RlZE5vZGVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJuZXctbXV0YXRpb25zXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJuZXdNdXRhdGlvbnNcIlxuICAgICAgICB9LFxuICAgICAgICBcInBpY2tlci1ub2RlLXBpY2tlZFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwicGlja2VyTm9kZVBpY2tlZFwiLFxuICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImRpc2Nvbm5lY3RlZE5vZGVcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJwaWNrZXItbm9kZS1ob3ZlcmVkXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJwaWNrZXJOb2RlSG92ZXJlZFwiLFxuICAgICAgICAgIFwibm9kZVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImRpc2Nvbm5lY3RlZE5vZGVcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJoaWdobGlnaHRlci1yZWFkeVwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwiaGlnaGxpZ2h0ZXItcmVhZHlcIlxuICAgICAgICB9LFxuICAgICAgICBcImhpZ2hsaWdodGVyLWhpZGVcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcImhpZ2hsaWdodGVyLWhpZGVcIlxuICAgICAgICB9XG4gICAgICB9XG4gICAgfSxcbiAgICBcImluc3BlY3RvclwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJpbnNwZWN0b3JcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRXYWxrZXJcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0V2Fsa2VyXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ3YWxrZXJcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJkb213YWxrZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFBhZ2VTdHlsZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRQYWdlU3R5bGVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInBhZ2VTdHlsZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcInBhZ2VzdHlsZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0SGlnaGxpZ2h0ZXJcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0SGlnaGxpZ2h0ZXJcIixcbiAgICAgICAgICAgIFwiYXV0b2hpZGVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiaGlnaGxpZ3RlclwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImhpZ2hsaWdodGVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRJbWFnZURhdGFGcm9tVVJMXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldEltYWdlRGF0YUZyb21VUkxcIixcbiAgICAgICAgICAgIFwidXJsXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJtYXhEaW1cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6bnVtYmVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiaW1hZ2VEYXRhXCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJjYWxsLXN0YWNrLWl0ZW1cIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJjYWxsLXN0YWNrLWl0ZW1cIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJuYW1lXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwiZmlsZVwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcImxpbmVcIjogXCJudW1iZXJcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJjYWxsLWRldGFpbHNcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJjYWxsLWRldGFpbHNcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJ0eXBlXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwibmFtZVwiOiBcInN0cmluZ1wiLFxuICAgICAgICBcInN0YWNrXCI6IFwiYXJyYXk6Y2FsbC1zdGFjay1pdGVtXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiZnVuY3Rpb24tY2FsbFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJmdW5jdGlvbi1jYWxsXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0RGV0YWlsc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXREZXRhaWxzXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJpbmZvXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiY2FsbC1kZXRhaWxzXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJjYWxsLXdhdGNoZXJcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiY2FsbC13YXRjaGVyXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2V0dXBcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXR1cFwiLFxuICAgICAgICAgICAgXCJ0cmFjZWRHbG9iYWxzXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmFycmF5OnN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJ0cmFjZWRGdW5jdGlvbnNcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6YXJyYXk6c3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInN0YXJ0UmVjb3JkaW5nXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicGVyZm9ybVJlbG9hZFwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImZpbmFsaXplXCIsXG4gICAgICAgICAgXCJvbmV3YXlcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZmluYWxpemVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiaXNSZWNvcmRpbmdcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiaXNSZWNvcmRpbmdcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJib29sZWFuXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJyZXN1bWVSZWNvcmRpbmdcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicmVzdW1lUmVjb3JkaW5nXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInBhdXNlUmVjb3JkaW5nXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInBhdXNlUmVjb3JkaW5nXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJjYWxsc1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImFycmF5OmZ1bmN0aW9uLWNhbGxcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImVyYXNlUmVjb3JkaW5nXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImVyYXNlUmVjb3JkaW5nXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcInNuYXBzaG90LWltYWdlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwic25hcHNob3QtaW1hZ2VcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJpbmRleFwiOiBcIm51bWJlclwiLFxuICAgICAgICBcIndpZHRoXCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwiaGVpZ2h0XCI6IFwibnVtYmVyXCIsXG4gICAgICAgIFwiZmxpcHBlZFwiOiBcImJvb2xlYW5cIixcbiAgICAgICAgXCJwaXhlbHNcIjogXCJ1aW50MzItYXJyYXlcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJzbmFwc2hvdC1vdmVydmlld1wiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInNuYXBzaG90LW92ZXJ2aWV3XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwiY2FsbHNcIjogXCJhcnJheTpmdW5jdGlvbi1jYWxsXCIsXG4gICAgICAgIFwidGh1bWJuYWlsc1wiOiBcImFycmF5OnNuYXBzaG90LWltYWdlXCIsXG4gICAgICAgIFwic2NyZWVuc2hvdFwiOiBcInNuYXBzaG90LWltYWdlXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwiZnJhbWUtc25hcHNob3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZnJhbWUtc25hcHNob3RcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRPdmVydmlld1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRPdmVydmlld1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwib3ZlcnZpZXdcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzbmFwc2hvdC1vdmVydmlld1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2VuZXJhdGVTY3JlZW5zaG90Rm9yXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdlbmVyYXRlU2NyZWVuc2hvdEZvclwiLFxuICAgICAgICAgICAgXCJjYWxsXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImZ1bmN0aW9uLWNhbGxcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInNjcmVlbnNob3RcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzbmFwc2hvdC1pbWFnZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiY2FudmFzXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImNhbnZhc1wiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNldHVwXCIsXG4gICAgICAgICAgXCJvbmV3YXlcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2V0dXBcIixcbiAgICAgICAgICAgIFwicmVsb2FkXCI6IHtcbiAgICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZmluYWxpemVcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJmaW5hbGl6ZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJpc0luaXRpYWxpemVkXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImlzSW5pdGlhbGl6ZWRcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcImluaXRpYWxpemVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwicmVjb3JkQW5pbWF0aW9uRnJhbWVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwicmVjb3JkQW5pbWF0aW9uRnJhbWVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInNuYXBzaG90XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiZnJhbWUtc25hcHNob3RcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcImdsLXNoYWRlclwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJnbC1zaGFkZXJcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRUZXh0XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFRleHRcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInRleHRcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImNvbXBpbGVcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiY29tcGlsZVwiLFxuICAgICAgICAgICAgXCJ0ZXh0XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiZXJyb3JcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTpqc29uXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJnbC1wcm9ncmFtXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImdsLXByb2dyYW1cIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRWZXJ0ZXhTaGFkZXJcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0VmVydGV4U2hhZGVyXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzaGFkZXJcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJnbC1zaGFkZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldEZyYWdtZW50U2hhZGVyXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldEZyYWdtZW50U2hhZGVyXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzaGFkZXJcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJnbC1zaGFkZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImhpZ2hsaWdodFwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImhpZ2hsaWdodFwiLFxuICAgICAgICAgICAgXCJ0aW50XCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcImFycmF5Om51bWJlclwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ1bmhpZ2hsaWdodFwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInVuaGlnaGxpZ2h0XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImJsYWNrYm94XCIsXG4gICAgICAgICAgXCJvbmV3YXlcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYmxhY2tib3hcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwidW5ibGFja2JveFwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInVuYmxhY2tib3hcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwid2ViZ2xcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwid2ViZ2xcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZXR1cFwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNldHVwXCIsXG4gICAgICAgICAgICBcInJlbG9hZFwiOiB7XG4gICAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImZpbmFsaXplXCIsXG4gICAgICAgICAgXCJvbmV3YXlcIjogdHJ1ZSxcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZmluYWxpemVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0UHJvZ3JhbXNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0UHJvZ3JhbXNcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInByb2dyYW1zXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwiYXJyYXk6Z2wtcHJvZ3JhbVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge1xuICAgICAgICBcInByb2dyYW0tbGlua2VkXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJwcm9ncmFtTGlua2VkXCIsXG4gICAgICAgICAgXCJwcm9ncmFtXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2wtcHJvZ3JhbVwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICB9XG4gICAgfSxcbiAgICBcImF1ZGlvbm9kZVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJhdWRpb25vZGVcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRUeXBlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFR5cGVcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImlzU291cmNlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImlzU291cmNlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzb3VyY2VcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZXRQYXJhbVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXRQYXJhbVwiLFxuICAgICAgICAgICAgXCJwYXJhbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6cHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJlcnJvclwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bGxhYmxlOmpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFBhcmFtXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFBhcmFtXCIsXG4gICAgICAgICAgICBcInBhcmFtXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidGV4dFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bGxhYmxlOnByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0UGFyYW1GbGFnc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRQYXJhbUZsYWdzXCIsXG4gICAgICAgICAgICBcInBhcmFtXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiZmxhZ3NcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJudWxsYWJsZTpwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFBhcmFtc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRQYXJhbXNcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInBhcmFtc1wiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcIndlYmF1ZGlvXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcIndlYmF1ZGlvXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2V0dXBcIixcbiAgICAgICAgICBcIm9uZXdheVwiOiB0cnVlLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJzZXR1cFwiLFxuICAgICAgICAgICAgXCJyZWxvYWRcIjoge1xuICAgICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYm9vbGVhblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJmaW5hbGl6ZVwiLFxuICAgICAgICAgIFwib25ld2F5XCI6IHRydWUsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImZpbmFsaXplXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJzdGFydC1jb250ZXh0XCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJzdGFydENvbnRleHRcIlxuICAgICAgICB9LFxuICAgICAgICBcImNvbm5lY3Qtbm9kZVwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwiY29ubmVjdE5vZGVcIixcbiAgICAgICAgICBcInNvdXJjZVwiOiB7XG4gICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImF1ZGlvbm9kZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcImRlc3RcIjoge1xuICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJhdWRpb25vZGVcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJkaXNjb25uZWN0LW5vZGVcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcImRpc2Nvbm5lY3ROb2RlXCIsXG4gICAgICAgICAgXCJzb3VyY2VcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJhdWRpb25vZGVcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJjb25uZWN0LXBhcmFtXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJjb25uZWN0UGFyYW1cIixcbiAgICAgICAgICBcInNvdXJjZVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImF1ZGlvbm9kZVwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInBhcmFtXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIFwiY2hhbmdlLXBhcmFtXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJjaGFuZ2VQYXJhbVwiLFxuICAgICAgICAgIFwic291cmNlXCI6IHtcbiAgICAgICAgICAgIFwiX29wdGlvblwiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYXVkaW9ub2RlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicGFyYW1cIjoge1xuICAgICAgICAgICAgXCJfb3B0aW9uXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICBcIl9vcHRpb25cIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICBcImNyZWF0ZS1ub2RlXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJjcmVhdGVOb2RlXCIsXG4gICAgICAgICAgXCJzb3VyY2VcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJhdWRpb25vZGVcIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJvbGQtc3R5bGVzaGVldFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJvbGQtc3R5bGVzaGVldFwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInRvZ2dsZURpc2FibGVkXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInRvZ2dsZURpc2FibGVkXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJkaXNhYmxlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImJvb2xlYW5cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImZldGNoU291cmNlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImZldGNoU291cmNlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInVwZGF0ZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ1cGRhdGVcIixcbiAgICAgICAgICAgIFwidGV4dFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwidHJhbnNpdGlvblwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJwcm9wZXJ0eS1jaGFuZ2VcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcInByb3BlcnR5Q2hhbmdlXCIsXG4gICAgICAgICAgXCJwcm9wZXJ0eVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICBcInNvdXJjZS1sb2FkXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJzb3VyY2VMb2FkXCIsXG4gICAgICAgICAgXCJzb3VyY2VcIjoge1xuICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJzdHlsZS1hcHBsaWVkXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJzdHlsZUFwcGxpZWRcIlxuICAgICAgICB9XG4gICAgICB9XG4gICAgfSxcbiAgICBcInN0eWxlZWRpdG9yXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInN0eWxlZWRpdG9yXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwibmV3RG9jdW1lbnRcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwibmV3RG9jdW1lbnRcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwibmV3U3R5bGVTaGVldFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJuZXdTdHlsZVNoZWV0XCIsXG4gICAgICAgICAgICBcInRleHRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzdHlsZVNoZWV0XCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwib2xkLXN0eWxlc2hlZXRcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHtcbiAgICAgICAgXCJkb2N1bWVudC1sb2FkXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJkb2N1bWVudExvYWRcIixcbiAgICAgICAgICBcInN0eWxlU2hlZXRzXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiYXJyYXk6b2xkLXN0eWxlc2hlZXRcIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgfVxuICAgIH0sXG4gICAgXCJjb29raWVvYmplY3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJjb29raWVvYmplY3RcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJuYW1lXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwidmFsdWVcIjogXCJsb25nc3RyaW5nXCIsXG4gICAgICAgIFwicGF0aFwiOiBcIm51bGxhYmxlOnN0cmluZ1wiLFxuICAgICAgICBcImhvc3RcIjogXCJzdHJpbmdcIixcbiAgICAgICAgXCJpc0RvbWFpblwiOiBcImJvb2xlYW5cIixcbiAgICAgICAgXCJpc1NlY3VyZVwiOiBcImJvb2xlYW5cIixcbiAgICAgICAgXCJpc0h0dHBPbmx5XCI6IFwiYm9vbGVhblwiLFxuICAgICAgICBcImNyZWF0aW9uVGltZVwiOiBcIm51bWJlclwiLFxuICAgICAgICBcImxhc3RBY2Nlc3NlZFwiOiBcIm51bWJlclwiLFxuICAgICAgICBcImV4cGlyZXNcIjogXCJudW1iZXJcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJjb29raWVzdG9yZW9iamVjdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImNvb2tpZXN0b3Jlb2JqZWN0XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwidG90YWxcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJvZmZzZXRcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJkYXRhXCI6IFwiYXJyYXk6bnVsbGFibGU6Y29va2llb2JqZWN0XCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwic3RvcmFnZW9iamVjdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcInN0b3JhZ2VvYmplY3RcIixcbiAgICAgIFwic3BlY2lhbGl6YXRpb25zXCI6IHtcbiAgICAgICAgXCJuYW1lXCI6IFwic3RyaW5nXCIsXG4gICAgICAgIFwidmFsdWVcIjogXCJsb25nc3RyaW5nXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwic3RvcmFnZXN0b3Jlb2JqZWN0XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwic3RvcmFnZXN0b3Jlb2JqZWN0XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwidG90YWxcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJvZmZzZXRcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJkYXRhXCI6IFwiYXJyYXk6bnVsbGFibGU6c3RvcmFnZW9iamVjdFwiXG4gICAgICB9XG4gICAgfSxcbiAgICBcImlkYm9iamVjdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImlkYm9iamVjdFwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcIm5hbWVcIjogXCJudWxsYWJsZTpzdHJpbmdcIixcbiAgICAgICAgXCJkYlwiOiBcIm51bGxhYmxlOnN0cmluZ1wiLFxuICAgICAgICBcIm9iamVjdFN0b3JlXCI6IFwibnVsbGFibGU6c3RyaW5nXCIsXG4gICAgICAgIFwib3JpZ2luXCI6IFwibnVsbGFibGU6c3RyaW5nXCIsXG4gICAgICAgIFwidmVyc2lvblwiOiBcIm51bGxhYmxlOm51bWJlclwiLFxuICAgICAgICBcIm9iamVjdFN0b3Jlc1wiOiBcIm51bGxhYmxlOm51bWJlclwiLFxuICAgICAgICBcImtleVBhdGhcIjogXCJudWxsYWJsZTpzdHJpbmdcIixcbiAgICAgICAgXCJhdXRvSW5jcmVtZW50XCI6IFwibnVsbGFibGU6Ym9vbGVhblwiLFxuICAgICAgICBcImluZGV4ZXNcIjogXCJudWxsYWJsZTpzdHJpbmdcIixcbiAgICAgICAgXCJ2YWx1ZVwiOiBcIm51bGxhYmxlOmxvbmdzdHJpbmdcIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJpZGJzdG9yZW9iamVjdFwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiZGljdFwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImlkYnN0b3Jlb2JqZWN0XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwidG90YWxcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJvZmZzZXRcIjogXCJudW1iZXJcIixcbiAgICAgICAgXCJkYXRhXCI6IFwiYXJyYXk6bnVsbGFibGU6aWRib2JqZWN0XCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwic3RvcmVVcGRhdGVPYmplY3RcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImRpY3RcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJzdG9yZVVwZGF0ZU9iamVjdFwiLFxuICAgICAgXCJzcGVjaWFsaXphdGlvbnNcIjoge1xuICAgICAgICBcImNoYW5nZWRcIjogXCJudWxsYWJsZTpqc29uXCIsXG4gICAgICAgIFwiZGVsZXRlZFwiOiBcIm51bGxhYmxlOmpzb25cIixcbiAgICAgICAgXCJhZGRlZFwiOiBcIm51bGxhYmxlOmpzb25cIlxuICAgICAgfVxuICAgIH0sXG4gICAgXCJjb29raWVzXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImNvb2tpZXNcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRTdG9yZU9iamVjdHNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0U3RvcmVPYmplY3RzXCIsXG4gICAgICAgICAgICBcImhvc3RcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm5hbWVzXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmFycmF5OnN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJvcHRpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDIsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJjb29raWVzdG9yZW9iamVjdFwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwibG9jYWxTdG9yYWdlXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImxvY2FsU3RvcmFnZVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldFN0b3JlT2JqZWN0c1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRTdG9yZU9iamVjdHNcIixcbiAgICAgICAgICAgIFwiaG9zdFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwibmFtZXNcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6YXJyYXk6c3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm9wdGlvbnNcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMixcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwibnVsbGFibGU6anNvblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcInN0b3JhZ2VzdG9yZW9iamVjdFwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwic2Vzc2lvblN0b3JhZ2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwic2Vzc2lvblN0b3JhZ2VcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRTdG9yZU9iamVjdHNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0U3RvcmVPYmplY3RzXCIsXG4gICAgICAgICAgICBcImhvc3RcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm5hbWVzXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmFycmF5OnN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJvcHRpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDIsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzdG9yYWdlc3RvcmVvYmplY3RcIlxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfSxcbiAgICBcImluZGV4ZWREQlwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJpbmRleGVkREJcIixcbiAgICAgIFwibWV0aG9kc1wiOiBbXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRTdG9yZU9iamVjdHNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0U3RvcmVPYmplY3RzXCIsXG4gICAgICAgICAgICBcImhvc3RcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcIm5hbWVzXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmFycmF5OnN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJvcHRpb25zXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDIsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bGxhYmxlOmpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJpZGJzdG9yZW9iamVjdFwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwic3RvcmVsaXN0XCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJkaWN0XCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwic3RvcmVsaXN0XCIsXG4gICAgICBcInNwZWNpYWxpemF0aW9uc1wiOiB7XG4gICAgICAgIFwiY29va2llc1wiOiBcImNvb2tpZXNcIixcbiAgICAgICAgXCJsb2NhbFN0b3JhZ2VcIjogXCJsb2NhbFN0b3JhZ2VcIixcbiAgICAgICAgXCJzZXNzaW9uU3RvcmFnZVwiOiBcInNlc3Npb25TdG9yYWdlXCIsXG4gICAgICAgIFwiaW5kZXhlZERCXCI6IFwiaW5kZXhlZERCXCJcbiAgICAgIH1cbiAgICB9LFxuICAgIFwic3RvcmFnZVwiOiB7XG4gICAgICBcImNhdGVnb3J5XCI6IFwiYWN0b3JcIixcbiAgICAgIFwidHlwZU5hbWVcIjogXCJzdG9yYWdlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwibGlzdFN0b3Jlc1wiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJsaXN0U3RvcmVzXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic3RvcmVsaXN0XCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7XG4gICAgICAgIFwic3RvcmVzLXVwZGF0ZVwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwic3RvcmVzVXBkYXRlXCIsXG4gICAgICAgICAgXCJkYXRhXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RvcmVVcGRhdGVPYmplY3RcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJzdG9yZXMtY2xlYXJlZFwiOiB7XG4gICAgICAgICAgXCJ0eXBlXCI6IFwic3RvcmVzQ2xlYXJlZFwiLFxuICAgICAgICAgIFwiZGF0YVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImpzb25cIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAgXCJzdG9yZXMtcmVsb2FkZWRcIjoge1xuICAgICAgICAgIFwidHlwZVwiOiBcInN0b3Jlc1JlbGFvZGVkXCIsXG4gICAgICAgICAgXCJkYXRhXCI6IHtcbiAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgXCJ0eXBlXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICB9XG4gICAgfSxcbiAgICBcImdjbGlcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZ2NsaVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNwZWNzXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNwZWNzXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZXhlY3V0ZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJleGVjdXRlXCIsXG4gICAgICAgICAgICBcInR5cGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInN0YXRlXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0YXRlXCIsXG4gICAgICAgICAgICBcInR5cGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJzdGFydFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudW1iZXJcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicmFua1wiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAyLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJudW1iZXJcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ0eXBlcGFyc2VcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwidHlwZXBhcnNlXCIsXG4gICAgICAgICAgICBcInR5cGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJwYXJhbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJ0eXBlaW5jcmVtZW50XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInR5cGVpbmNyZW1lbnRcIixcbiAgICAgICAgICAgIFwidHlwZWRcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInBhcmFtXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwidHlwZWRlY3JlbWVudFwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJ0eXBlZGVjcmVtZW50XCIsXG4gICAgICAgICAgICBcInR5cGVkXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInN0cmluZ1wiXG4gICAgICAgICAgICB9LFxuICAgICAgICAgICAgXCJwYXJhbVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJzdHJpbmdcIlxuICAgICAgICAgIH1cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNlbGVjdGlvbmluZm9cIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2VsZWN0aW9uaW5mb1wiLFxuICAgICAgICAgICAgXCJ0eXBlZFwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwicGFyYW1cIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcImFjdGlvblwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAxLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJzdHJpbmdcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJqc29uXCJcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJtZW1vcnlcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwibWVtb3J5XCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwibWVhc3VyZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJtZWFzdXJlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge31cbiAgICB9LFxuICAgIFwiZXZlbnRMb29wTGFnXCI6IHtcbiAgICAgIFwiY2F0ZWdvcnlcIjogXCJhY3RvclwiLFxuICAgICAgXCJ0eXBlTmFtZVwiOiBcImV2ZW50TG9vcExhZ1wiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInN0YXJ0XCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0YXJ0XCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJzdWNjZXNzXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibnVtYmVyXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzdG9wXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInN0b3BcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7fVxuICAgICAgICB9XG4gICAgICBdLFxuICAgICAgXCJldmVudHNcIjoge1xuICAgICAgICBcImV2ZW50LWxvb3AtbGFnXCI6IHtcbiAgICAgICAgICBcInR5cGVcIjogXCJldmVudC1sb29wLWxhZ1wiLFxuICAgICAgICAgIFwidGltZVwiOiB7XG4gICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgIFwidHlwZVwiOiBcIm51bWJlclwiXG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICB9XG4gICAgfSxcbiAgICBcInByZWZlcmVuY2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwicHJlZmVyZW5jZVwiLFxuICAgICAgXCJtZXRob2RzXCI6IFtcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImdldEJvb2xQcmVmXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldEJvb2xQcmVmXCIsXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJib29sZWFuXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRDaGFyUHJlZlwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRDaGFyUHJlZlwiLFxuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwic3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJnZXRJbnRQcmVmXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldEludFByZWZcIixcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcIm51bWJlclwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0QWxsUHJlZnNcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0QWxsUHJlZnNcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwic2V0Qm9vbFByZWZcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwic2V0Qm9vbFByZWZcIixcbiAgICAgICAgICAgIFwibmFtZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcInNldENoYXJQcmVmXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNldENoYXJQcmVmXCIsXG4gICAgICAgICAgICBcIm5hbWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMCxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDEsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzZXRJbnRQcmVmXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNldEludFByZWZcIixcbiAgICAgICAgICAgIFwibmFtZVwiOiB7XG4gICAgICAgICAgICAgIFwiX2FyZ1wiOiAwLFxuICAgICAgICAgICAgICBcInR5cGVcIjogXCJwcmltaXRpdmVcIlxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9hcmdcIjogMSxcbiAgICAgICAgICAgICAgXCJ0eXBlXCI6IFwicHJpbWl0aXZlXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge31cbiAgICAgICAgfSxcbiAgICAgICAge1xuICAgICAgICAgIFwibmFtZVwiOiBcImNsZWFyVXNlclByZWZcIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiY2xlYXJVc2VyUHJlZlwiLFxuICAgICAgICAgICAgXCJuYW1lXCI6IHtcbiAgICAgICAgICAgICAgXCJfYXJnXCI6IDAsXG4gICAgICAgICAgICAgIFwidHlwZVwiOiBcInByaW1pdGl2ZVwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHt9XG4gICAgICAgIH1cbiAgICAgIF0sXG4gICAgICBcImV2ZW50c1wiOiB7fVxuICAgIH0sXG4gICAgXCJkZXZpY2VcIjoge1xuICAgICAgXCJjYXRlZ29yeVwiOiBcImFjdG9yXCIsXG4gICAgICBcInR5cGVOYW1lXCI6IFwiZGV2aWNlXCIsXG4gICAgICBcIm1ldGhvZHNcIjogW1xuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0RGVzY3JpcHRpb25cIixcbiAgICAgICAgICBcInJlcXVlc3RcIjoge1xuICAgICAgICAgICAgXCJ0eXBlXCI6IFwiZ2V0RGVzY3JpcHRpb25cIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwianNvblwiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0V2FsbHBhcGVyXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcImdldFdhbGxwYXBlclwiXG4gICAgICAgICAgfSxcbiAgICAgICAgICBcInJlc3BvbnNlXCI6IHtcbiAgICAgICAgICAgIFwidmFsdWVcIjoge1xuICAgICAgICAgICAgICBcIl9yZXR2YWxcIjogXCJsb25nc3RyaW5nXCJcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH0sXG4gICAgICAgIHtcbiAgICAgICAgICBcIm5hbWVcIjogXCJzY3JlZW5zaG90VG9EYXRhVVJMXCIsXG4gICAgICAgICAgXCJyZXF1ZXN0XCI6IHtcbiAgICAgICAgICAgIFwidHlwZVwiOiBcInNjcmVlbnNob3RUb0RhdGFVUkxcIlxuICAgICAgICAgIH0sXG4gICAgICAgICAgXCJyZXNwb25zZVwiOiB7XG4gICAgICAgICAgICBcInZhbHVlXCI6IHtcbiAgICAgICAgICAgICAgXCJfcmV0dmFsXCI6IFwibG9uZ3N0cmluZ1wiXG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICB9LFxuICAgICAgICB7XG4gICAgICAgICAgXCJuYW1lXCI6IFwiZ2V0UmF3UGVybWlzc2lvbnNUYWJsZVwiLFxuICAgICAgICAgIFwicmVxdWVzdFwiOiB7XG4gICAgICAgICAgICBcInR5cGVcIjogXCJnZXRSYXdQZXJtaXNzaW9uc1RhYmxlXCJcbiAgICAgICAgICB9LFxuICAgICAgICAgIFwicmVzcG9uc2VcIjoge1xuICAgICAgICAgICAgXCJ2YWx1ZVwiOiB7XG4gICAgICAgICAgICAgIFwiX3JldHZhbFwiOiBcImpzb25cIlxuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgXSxcbiAgICAgIFwiZXZlbnRzXCI6IHt9XG4gICAgfVxuICB9LFxuICBcImZyb21cIjogXCJyb290XCJcbn1cbiIsIlwidXNlIHN0cmljdFwiO1xuXG52YXIgQ2xhc3MgPSByZXF1aXJlKFwiLi9jbGFzc1wiKS5DbGFzcztcbnZhciB1dGlsID0gcmVxdWlyZShcIi4vdXRpbFwiKTtcbnZhciBrZXlzID0gdXRpbC5rZXlzO1xudmFyIHZhbHVlcyA9IHV0aWwudmFsdWVzO1xudmFyIHBhaXJzID0gdXRpbC5wYWlycztcbnZhciBxdWVyeSA9IHV0aWwucXVlcnk7XG52YXIgZmluZFBhdGggPSB1dGlsLmZpbmRQYXRoO1xudmFyIEV2ZW50VGFyZ2V0ID0gcmVxdWlyZShcIi4vZXZlbnRcIikuRXZlbnRUYXJnZXQ7XG5cbnZhciBUeXBlU3lzdGVtID0gQ2xhc3Moe1xuICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oY2xpZW50KSB7XG4gICAgdmFyIHR5cGVzID0gT2JqZWN0LmNyZWF0ZShudWxsKTtcbiAgICB2YXIgc3BlY2lmaWNhdGlvbiA9IE9iamVjdC5jcmVhdGUobnVsbCk7XG5cbiAgICB0aGlzLnNwZWNpZmljYXRpb24gPSBzcGVjaWZpY2F0aW9uO1xuICAgIHRoaXMudHlwZXMgPSB0eXBlcztcblxuICAgIHZhciB0eXBlRm9yID0gZnVuY3Rpb24gdHlwZUZvcih0eXBlTmFtZSkge1xuICAgICAgdHlwZU5hbWUgPSB0eXBlTmFtZSB8fCBcInByaW1pdGl2ZVwiO1xuICAgICAgaWYgKCF0eXBlc1t0eXBlTmFtZV0pIHtcbiAgICAgICAgZGVmaW5lVHlwZSh0eXBlTmFtZSk7XG4gICAgICB9XG5cbiAgICAgIHJldHVybiB0eXBlc1t0eXBlTmFtZV07XG4gICAgfTtcbiAgICB0aGlzLnR5cGVGb3IgPSB0eXBlRm9yO1xuXG4gICAgdmFyIGRlZmluZVR5cGUgPSBmdW5jdGlvbihkZXNjcmlwdG9yKSB7XG4gICAgICB2YXIgdHlwZSA9IHZvaWQoMCk7XG4gICAgICBpZiAodHlwZW9mKGRlc2NyaXB0b3IpID09PSBcInN0cmluZ1wiKSB7XG4gICAgICAgIGlmIChkZXNjcmlwdG9yLmluZGV4T2YoXCI6XCIpID4gMClcbiAgICAgICAgICB0eXBlID0gbWFrZUNvbXBvdW5kVHlwZShkZXNjcmlwdG9yKTtcbiAgICAgICAgZWxzZSBpZiAoZGVzY3JpcHRvci5pbmRleE9mKFwiI1wiKSA+IDApXG4gICAgICAgICAgdHlwZSA9IG5ldyBBY3RvckRldGFpbChkZXNjcmlwdG9yKTtcbiAgICAgICAgICBlbHNlIGlmIChzcGVjaWZpY2F0aW9uW2Rlc2NyaXB0b3JdKVxuICAgICAgICAgICAgdHlwZSA9IG1ha2VDYXRlZ29yeVR5cGUoc3BlY2lmaWNhdGlvbltkZXNjcmlwdG9yXSk7XG4gICAgICB9IGVsc2Uge1xuICAgICAgICB0eXBlID0gbWFrZUNhdGVnb3J5VHlwZShkZXNjcmlwdG9yKTtcbiAgICAgIH1cblxuICAgICAgaWYgKHR5cGUpXG4gICAgICAgIHR5cGVzW3R5cGUubmFtZV0gPSB0eXBlO1xuICAgICAgZWxzZVxuICAgICAgICB0aHJvdyBUeXBlRXJyb3IoXCJJbnZhbGlkIHR5cGU6IFwiICsgZGVzY3JpcHRvcik7XG4gICAgfTtcbiAgICB0aGlzLmRlZmluZVR5cGUgPSBkZWZpbmVUeXBlO1xuXG5cbiAgICB2YXIgbWFrZUNvbXBvdW5kVHlwZSA9IGZ1bmN0aW9uKG5hbWUpIHtcbiAgICAgIHZhciBpbmRleCA9IG5hbWUuaW5kZXhPZihcIjpcIik7XG4gICAgICB2YXIgYmFzZVR5cGUgPSBuYW1lLnNsaWNlKDAsIGluZGV4KTtcbiAgICAgIHZhciBzdWJUeXBlID0gbmFtZS5zbGljZShpbmRleCArIDEpO1xuXG4gICAgICByZXR1cm4gYmFzZVR5cGUgPT09IFwiYXJyYXlcIiA/IG5ldyBBcnJheU9mKHN1YlR5cGUpIDpcbiAgICAgIGJhc2VUeXBlID09PSBcIm51bGxhYmxlXCIgPyBuZXcgTWF5YmUoc3ViVHlwZSkgOlxuICAgICAgbnVsbDtcbiAgICB9O1xuXG4gICAgdmFyIG1ha2VDYXRlZ29yeVR5cGUgPSBmdW5jdGlvbihkZXNjcmlwdG9yKSB7XG4gICAgICB2YXIgY2F0ZWdvcnkgPSBkZXNjcmlwdG9yLmNhdGVnb3J5O1xuICAgICAgcmV0dXJuIGNhdGVnb3J5ID09PSBcImRpY3RcIiA/IG5ldyBEaWN0aW9uYXJ5KGRlc2NyaXB0b3IpIDpcbiAgICAgIGNhdGVnb3J5ID09PSBcImFjdG9yXCIgPyBuZXcgQWN0b3IoZGVzY3JpcHRvcikgOlxuICAgICAgbnVsbDtcbiAgICB9O1xuXG4gICAgdmFyIHJlYWQgPSBmdW5jdGlvbihpbnB1dCwgY29udGV4dCwgdHlwZU5hbWUpIHtcbiAgICAgIHJldHVybiB0eXBlRm9yKHR5cGVOYW1lKS5yZWFkKGlucHV0LCBjb250ZXh0KTtcbiAgICB9XG4gICAgdGhpcy5yZWFkID0gcmVhZDtcblxuICAgIHZhciB3cml0ZSA9IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0LCB0eXBlTmFtZSkge1xuICAgICAgcmV0dXJuIHR5cGVGb3IodHlwZU5hbWUpLndyaXRlKGlucHV0KTtcbiAgICB9O1xuICAgIHRoaXMud3JpdGUgPSB3cml0ZTtcblxuXG4gICAgdmFyIFR5cGUgPSBDbGFzcyh7XG4gICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oKSB7XG4gICAgICB9LFxuICAgICAgZ2V0IG5hbWUoKSB7XG4gICAgICAgIHJldHVybiB0aGlzLmNhdGVnb3J5ID8gdGhpcy5jYXRlZ29yeSArIFwiOlwiICsgdGhpcy50eXBlIDpcbiAgICAgICAgdGhpcy50eXBlO1xuICAgICAgfSxcbiAgICAgIHJlYWQ6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHRocm93IG5ldyBUeXBlRXJyb3IoXCJgVHlwZWAgc3ViY2xhc3MgbXVzdCBpbXBsZW1lbnQgYHJlYWRgXCIpO1xuICAgICAgfSxcbiAgICAgIHdyaXRlOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICB0aHJvdyBuZXcgVHlwZUVycm9yKFwiYFR5cGVgIHN1YmNsYXNzIG11c3QgaW1wbGVtZW50IGB3cml0ZWBcIik7XG4gICAgICB9XG4gICAgfSk7XG5cbiAgICB2YXIgUHJpbWl0dmUgPSBDbGFzcyh7XG4gICAgICBleHRlbmRzOiBUeXBlLFxuICAgICAgY29uc3R1Y3RvcjogZnVuY3Rpb24odHlwZSkge1xuICAgICAgICB0aGlzLnR5cGUgPSB0eXBlO1xuICAgICAgfSxcbiAgICAgIHJlYWQ6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHJldHVybiBpbnB1dDtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgcmV0dXJuIGlucHV0O1xuICAgICAgfVxuICAgIH0pO1xuXG4gICAgdmFyIE1heWJlID0gQ2xhc3Moe1xuICAgICAgZXh0ZW5kczogVHlwZSxcbiAgICAgIGNhdGVnb3J5OiBcIm51bGxhYmxlXCIsXG4gICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24odHlwZSkge1xuICAgICAgICB0aGlzLnR5cGUgPSB0eXBlO1xuICAgICAgfSxcbiAgICAgIHJlYWQ6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHJldHVybiBpbnB1dCA9PT0gbnVsbCA/IG51bGwgOlxuICAgICAgICBpbnB1dCA9PT0gdm9pZCgwKSA/IHZvaWQoMCkgOlxuICAgICAgICByZWFkKGlucHV0LCBjb250ZXh0LCB0aGlzLnR5cGUpO1xuICAgICAgfSxcbiAgICAgIHdyaXRlOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICByZXR1cm4gaW5wdXQgPT09IG51bGwgPyBudWxsIDpcbiAgICAgICAgaW5wdXQgPT09IHZvaWQoMCkgPyB2b2lkKDApIDpcbiAgICAgICAgd3JpdGUoaW5wdXQsIGNvbnRleHQsIHRoaXMudHlwZSk7XG4gICAgICB9XG4gICAgfSk7XG5cbiAgICB2YXIgQXJyYXlPZiA9IENsYXNzKHtcbiAgICAgIGV4dGVuZHM6IFR5cGUsXG4gICAgICBjYXRlZ29yeTogXCJhcnJheVwiLFxuICAgICAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKHR5cGUpIHtcbiAgICAgICAgdGhpcy50eXBlID0gdHlwZTtcbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICB2YXIgdHlwZSA9IHRoaXMudHlwZTtcbiAgICAgICAgcmV0dXJuIGlucHV0Lm1hcChmdW5jdGlvbigkKSB7IHJldHVybiByZWFkKCQsIGNvbnRleHQsIHR5cGUpIH0pO1xuICAgICAgfSxcbiAgICAgIHdyaXRlOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICB2YXIgdHlwZSA9IHRoaXMudHlwZTtcbiAgICAgICAgcmV0dXJuIGlucHV0Lm1hcChmdW5jdGlvbigkKSB7IHJldHVybiB3cml0ZSgkLCBjb250ZXh0LCB0eXBlKSB9KTtcbiAgICAgIH1cbiAgICB9KTtcblxuICAgIHZhciBtYWtlRmllbGQgPSBmdW5jdGlvbiBtYWtlRmllbGQobmFtZSwgdHlwZSkge1xuICAgICAgcmV0dXJuIHtcbiAgICAgICAgZW51bWVyYWJsZTogdHJ1ZSxcbiAgICAgICAgY29uZmlndXJhYmxlOiB0cnVlLFxuICAgICAgICBnZXQ6IGZ1bmN0aW9uKCkge1xuICAgICAgICAgIE9iamVjdC5kZWZpbmVQcm9wZXJ0eSh0aGlzLCBuYW1lLCB7XG4gICAgICAgICAgICBjb25maWd1cmFibGU6IGZhbHNlLFxuICAgICAgICAgICAgdmFsdWU6IHJlYWQodGhpcy5zdGF0ZVtuYW1lXSwgdGhpcy5jb250ZXh0LCB0eXBlKVxuICAgICAgICAgIH0pO1xuICAgICAgICAgIHJldHVybiB0aGlzW25hbWVdO1xuICAgICAgICB9XG4gICAgICB9XG4gICAgfTtcblxuICAgIHZhciBtYWtlRmllbGRzID0gZnVuY3Rpb24oZGVzY3JpcHRvcikge1xuICAgICAgcmV0dXJuIHBhaXJzKGRlc2NyaXB0b3IpLnJlZHVjZShmdW5jdGlvbihmaWVsZHMsIHBhaXIpIHtcbiAgICAgICAgdmFyIG5hbWUgPSBwYWlyWzBdLCB0eXBlID0gcGFpclsxXTtcbiAgICAgICAgZmllbGRzW25hbWVdID0gbWFrZUZpZWxkKG5hbWUsIHR5cGUpO1xuICAgICAgICByZXR1cm4gZmllbGRzO1xuICAgICAgfSwge30pO1xuICAgIH1cblxuICAgIHZhciBEaWN0aW9uYXJ5VHlwZSA9IENsYXNzKHt9KTtcblxuICAgIHZhciBEaWN0aW9uYXJ5ID0gQ2xhc3Moe1xuICAgICAgZXh0ZW5kczogVHlwZSxcbiAgICAgIGNhdGVnb3J5OiBcImRpY3RcIixcbiAgICAgIGdldCBuYW1lKCkgeyByZXR1cm4gdGhpcy50eXBlOyB9LFxuICAgICAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKGRlc2NyaXB0b3IpIHtcbiAgICAgICAgdGhpcy50eXBlID0gZGVzY3JpcHRvci50eXBlTmFtZTtcbiAgICAgICAgdGhpcy50eXBlcyA9IGRlc2NyaXB0b3Iuc3BlY2lhbGl6YXRpb25zO1xuXG4gICAgICAgIHZhciBwcm90byA9IE9iamVjdC5kZWZpbmVQcm9wZXJ0aWVzKHtcbiAgICAgICAgICBleHRlbmRzOiBEaWN0aW9uYXJ5VHlwZSxcbiAgICAgICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oc3RhdGUsIGNvbnRleHQpIHtcbiAgICAgICAgICAgIE9iamVjdC5kZWZpbmVQcm9wZXJ0aWVzKHRoaXMsIHtcbiAgICAgICAgICAgICAgc3RhdGU6IHtcbiAgICAgICAgICAgICAgICBlbnVtZXJhYmxlOiBmYWxzZSxcbiAgICAgICAgICAgICAgICB3cml0YWJsZTogdHJ1ZSxcbiAgICAgICAgICAgICAgICBjb25maWd1cmFibGU6IHRydWUsXG4gICAgICAgICAgICAgICAgdmFsdWU6IHN0YXRlXG4gICAgICAgICAgICAgIH0sXG4gICAgICAgICAgICAgIGNvbnRleHQ6IHtcbiAgICAgICAgICAgICAgICBlbnVtZXJhYmxlOiBmYWxzZSxcbiAgICAgICAgICAgICAgICB3cml0YWJsZTogZmFsc2UsXG4gICAgICAgICAgICAgICAgY29uZmlndXJhYmxlOiB0cnVlLFxuICAgICAgICAgICAgICAgIHZhbHVlOiBjb250ZXh0XG4gICAgICAgICAgICAgIH1cbiAgICAgICAgICAgIH0pO1xuICAgICAgICAgIH1cbiAgICAgICAgfSwgbWFrZUZpZWxkcyh0aGlzLnR5cGVzKSk7XG5cbiAgICAgICAgdGhpcy5jbGFzcyA9IG5ldyBDbGFzcyhwcm90byk7XG4gICAgICB9LFxuICAgICAgcmVhZDogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgcmV0dXJuIG5ldyB0aGlzLmNsYXNzKGlucHV0LCBjb250ZXh0KTtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQpIHtcbiAgICAgICAgdmFyIG91dHB1dCA9IHt9O1xuICAgICAgICBmb3IgKHZhciBrZXkgaW4gaW5wdXQpIHtcbiAgICAgICAgICBvdXRwdXRba2V5XSA9IHdyaXRlKHZhbHVlLCBjb250ZXh0LCB0eXBlc1trZXldKTtcbiAgICAgICAgfVxuICAgICAgICByZXR1cm4gb3V0cHV0O1xuICAgICAgfVxuICAgIH0pO1xuXG4gICAgdmFyIG1ha2VNZXRob2RzID0gZnVuY3Rpb24oZGVzY3JpcHRvcnMpIHtcbiAgICAgIHJldHVybiBkZXNjcmlwdG9ycy5yZWR1Y2UoZnVuY3Rpb24obWV0aG9kcywgZGVzY3JpcHRvcikge1xuICAgICAgICBtZXRob2RzW2Rlc2NyaXB0b3IubmFtZV0gPSB7XG4gICAgICAgICAgZW51bWVyYWJsZTogdHJ1ZSxcbiAgICAgICAgICBjb25maWd1cmFibGU6IHRydWUsXG4gICAgICAgICAgd3JpdGFibGU6IGZhbHNlLFxuICAgICAgICAgIHZhbHVlOiBtYWtlTWV0aG9kKGRlc2NyaXB0b3IpXG4gICAgICAgIH07XG4gICAgICAgIHJldHVybiBtZXRob2RzO1xuICAgICAgfSwge30pO1xuICAgIH07XG5cbiAgICB2YXIgbWFrZUV2ZW50cyA9IGZ1bmN0aW9uKGRlc2NyaXB0b3JzKSB7XG4gICAgICByZXR1cm4gcGFpcnMoZGVzY3JpcHRvcnMpLnJlZHVjZShmdW5jdGlvbihldmVudHMsIHBhaXIpIHtcbiAgICAgICAgdmFyIG5hbWUgPSBwYWlyWzBdLCBkZXNjcmlwdG9yID0gcGFpclsxXTtcbiAgICAgICAgdmFyIGV2ZW50ID0gbmV3IEV2ZW50KG5hbWUsIGRlc2NyaXB0b3IpO1xuICAgICAgICBldmVudHNbZXZlbnQuZXZlbnRUeXBlXSA9IGV2ZW50O1xuICAgICAgICByZXR1cm4gZXZlbnRzO1xuICAgICAgfSwgT2JqZWN0LmNyZWF0ZShudWxsKSk7XG4gICAgfTtcblxuICAgIHZhciBBY3RvciA9IENsYXNzKHtcbiAgICAgIGV4dGVuZHM6IFR5cGUsXG4gICAgICBjYXRlZ29yeTogXCJhY3RvclwiLFxuICAgICAgZ2V0IG5hbWUoKSB7IHJldHVybiB0aGlzLnR5cGU7IH0sXG4gICAgICBjb25zdHJ1Y3RvcjogZnVuY3Rpb24oZGVzY3JpcHRvcikge1xuICAgICAgICB0aGlzLnR5cGUgPSBkZXNjcmlwdG9yLnR5cGVOYW1lO1xuXG4gICAgICAgIHZhciBldmVudHMgPSBtYWtlRXZlbnRzKGRlc2NyaXB0b3IuZXZlbnRzIHx8IHt9KTtcbiAgICAgICAgdmFyIGZpZWxkcyA9IG1ha2VGaWVsZHMoZGVzY3JpcHRvci5maWVsZHMgfHwge30pO1xuICAgICAgICB2YXIgbWV0aG9kcyA9IG1ha2VNZXRob2RzKGRlc2NyaXB0b3IubWV0aG9kcyB8fCBbXSk7XG5cblxuICAgICAgICB2YXIgcHJvdG8gPSB7XG4gICAgICAgICAgZXh0ZW5kczogRnJvbnQsXG4gICAgICAgICAgY29uc3RydWN0b3I6IGZ1bmN0aW9uKCkge1xuICAgICAgICAgICAgRnJvbnQuYXBwbHkodGhpcywgYXJndW1lbnRzKTtcbiAgICAgICAgICB9LFxuICAgICAgICAgIGV2ZW50czogZXZlbnRzXG4gICAgICAgIH07XG4gICAgICAgIE9iamVjdC5kZWZpbmVQcm9wZXJ0aWVzKHByb3RvLCBmaWVsZHMpO1xuICAgICAgICBPYmplY3QuZGVmaW5lUHJvcGVydGllcyhwcm90bywgbWV0aG9kcyk7XG5cbiAgICAgICAgdGhpcy5jbGFzcyA9IENsYXNzKHByb3RvKTtcbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCwgZGV0YWlsKSB7XG4gICAgICAgIHZhciBzdGF0ZSA9IHR5cGVvZihpbnB1dCkgPT09IFwic3RyaW5nXCIgPyB7IGFjdG9yOiBpbnB1dCB9IDogaW5wdXQ7XG5cbiAgICAgICAgdmFyIGFjdG9yID0gY2xpZW50LmdldChzdGF0ZS5hY3RvcikgfHwgbmV3IHRoaXMuY2xhc3Moc3RhdGUsIGNvbnRleHQpO1xuICAgICAgICBhY3Rvci5mb3JtKHN0YXRlLCBkZXRhaWwsIGNvbnRleHQpO1xuXG4gICAgICAgIHJldHVybiBhY3RvcjtcbiAgICAgIH0sXG4gICAgICB3cml0ZTogZnVuY3Rpb24oaW5wdXQsIGNvbnRleHQsIGRldGFpbCkge1xuICAgICAgICByZXR1cm4gaW5wdXQuaWQ7XG4gICAgICB9XG4gICAgfSk7XG4gICAgZXhwb3J0cy5BY3RvciA9IEFjdG9yO1xuXG5cbiAgICB2YXIgQWN0b3JEZXRhaWwgPSBDbGFzcyh7XG4gICAgICBleHRlbmRzOiBBY3RvcixcbiAgICAgIGNvbnN0cnVjdG9yOiBmdW5jdGlvbihuYW1lKSB7XG4gICAgICAgIHZhciBwYXJ0cyA9IG5hbWUuc3BsaXQoXCIjXCIpXG4gICAgICAgIHRoaXMuYWN0b3JUeXBlID0gcGFydHNbMF1cbiAgICAgICAgdGhpcy5kZXRhaWwgPSBwYXJ0c1sxXTtcbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICByZXR1cm4gdHlwZUZvcih0aGlzLmFjdG9yVHlwZSkucmVhZChpbnB1dCwgY29udGV4dCwgdGhpcy5kZXRhaWwpO1xuICAgICAgfSxcbiAgICAgIHdyaXRlOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICByZXR1cm4gdHlwZUZvcih0aGlzLmFjdG9yVHlwZSkud3JpdGUoaW5wdXQsIGNvbnRleHQsIHRoaXMuZGV0YWlsKTtcbiAgICAgIH1cbiAgICB9KTtcbiAgICBleHBvcnRzLkFjdG9yRGV0YWlsID0gQWN0b3JEZXRhaWw7XG5cbiAgICB2YXIgTWV0aG9kID0gQ2xhc3Moe1xuICAgICAgZXh0ZW5kczogVHlwZSxcbiAgICAgIGNvbnN0cnVjdG9yOiBmdW5jdGlvbihkZXNjcmlwdG9yKSB7XG4gICAgICAgIHRoaXMudHlwZSA9IGRlc2NyaXB0b3IubmFtZTtcbiAgICAgICAgdGhpcy5wYXRoID0gZmluZFBhdGgoZGVzY3JpcHRvci5yZXNwb25zZSwgXCJfcmV0dmFsXCIpO1xuICAgICAgICB0aGlzLnJlc3BvbnNlVHlwZSA9IHRoaXMucGF0aCAmJiBxdWVyeShkZXNjcmlwdG9yLnJlc3BvbnNlLCB0aGlzLnBhdGgpLl9yZXR2YWw7XG4gICAgICAgIHRoaXMucmVxdWVzdFR5cGUgPSBkZXNjcmlwdG9yLnJlcXVlc3QudHlwZTtcblxuICAgICAgICB2YXIgcGFyYW1zID0gW107XG4gICAgICAgIGZvciAodmFyIGtleSBpbiBkZXNjcmlwdG9yLnJlcXVlc3QpIHtcbiAgICAgICAgICBpZiAoa2V5ICE9PSBcInR5cGVcIikge1xuICAgICAgICAgICAgdmFyIHBhcmFtID0gZGVzY3JpcHRvci5yZXF1ZXN0W2tleV07XG4gICAgICAgICAgICB2YXIgaW5kZXggPSBcIl9hcmdcIiBpbiBwYXJhbSA/IHBhcmFtLl9hcmcgOiBwYXJhbS5fb3B0aW9uO1xuICAgICAgICAgICAgdmFyIGlzUGFyYW0gPSBwYXJhbS5fb3B0aW9uID09PSBpbmRleDtcbiAgICAgICAgICAgIHZhciBpc0FyZ3VtZW50ID0gcGFyYW0uX2FyZyA9PT0gaW5kZXg7XG4gICAgICAgICAgICBwYXJhbXNbaW5kZXhdID0ge1xuICAgICAgICAgICAgICB0eXBlOiBwYXJhbS50eXBlLFxuICAgICAgICAgICAgICBrZXk6IGtleSxcbiAgICAgICAgICAgICAgaW5kZXg6IGluZGV4LFxuICAgICAgICAgICAgICBpc1BhcmFtOiBpc1BhcmFtLFxuICAgICAgICAgICAgICBpc0FyZ3VtZW50OiBpc0FyZ3VtZW50XG4gICAgICAgICAgICB9O1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgICB0aGlzLnBhcmFtcyA9IHBhcmFtcztcbiAgICAgIH0sXG4gICAgICByZWFkOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICByZXR1cm4gcmVhZChxdWVyeShpbnB1dCwgdGhpcy5wYXRoKSwgY29udGV4dCwgdGhpcy5yZXNwb25zZVR5cGUpO1xuICAgICAgfSxcbiAgICAgIHdyaXRlOiBmdW5jdGlvbihpbnB1dCwgY29udGV4dCkge1xuICAgICAgICByZXR1cm4gdGhpcy5wYXJhbXMucmVkdWNlKGZ1bmN0aW9uKHJlc3VsdCwgcGFyYW0pIHtcbiAgICAgICAgICByZXN1bHRbcGFyYW0ua2V5XSA9IHdyaXRlKGlucHV0W3BhcmFtLmluZGV4XSwgY29udGV4dCwgcGFyYW0udHlwZSk7XG4gICAgICAgICAgcmV0dXJuIHJlc3VsdDtcbiAgICAgICAgfSwge3R5cGU6IHRoaXMudHlwZX0pO1xuICAgICAgfVxuICAgIH0pO1xuICAgIGV4cG9ydHMuTWV0aG9kID0gTWV0aG9kO1xuXG4gICAgdmFyIHByb2ZpbGVyID0gZnVuY3Rpb24obWV0aG9kLCBpZCkge1xuICAgICAgcmV0dXJuIGZ1bmN0aW9uKCkge1xuICAgICAgICB2YXIgc3RhcnQgPSBuZXcgRGF0ZSgpO1xuICAgICAgICByZXR1cm4gbWV0aG9kLmFwcGx5KHRoaXMsIGFyZ3VtZW50cykudGhlbihmdW5jdGlvbihyZXN1bHQpIHtcbiAgICAgICAgICB2YXIgZW5kID0gbmV3IERhdGUoKTtcbiAgICAgICAgICBjbGllbnQudGVsZW1ldHJ5LmFkZChpZCwgK2VuZCAtIHN0YXJ0KTtcbiAgICAgICAgICByZXR1cm4gcmVzdWx0O1xuICAgICAgICB9KTtcbiAgICAgIH07XG4gICAgfTtcblxuICAgIHZhciBkZXN0cnVjdG9yID0gZnVuY3Rpb24obWV0aG9kKSB7XG4gICAgICByZXR1cm4gZnVuY3Rpb24oKSB7XG4gICAgICAgIHJldHVybiBtZXRob2QuYXBwbHkodGhpcywgYXJndW1lbnRzKS50aGVuKGZ1bmN0aW9uKHJlc3VsdCkge1xuICAgICAgICAgIGNsaWVudC5yZWxlYXNlKHRoaXMpO1xuICAgICAgICAgIHJldHVybiByZXN1bHQ7XG4gICAgICAgIH0pO1xuICAgICAgfTtcbiAgICB9O1xuXG4gICAgZnVuY3Rpb24gbWFrZU1ldGhvZChkZXNjcmlwdG9yKSB7XG4gICAgICB2YXIgdHlwZSA9IG5ldyBNZXRob2QoZGVzY3JpcHRvcik7XG4gICAgICB2YXIgbWV0aG9kID0gZGVzY3JpcHRvci5vbmV3YXkgPyBtYWtlVW5pZGlyZWNhdGlvbmFsTWV0aG9kKGRlc2NyaXB0b3IsIHR5cGUpIDpcbiAgICAgICAgICAgICAgICAgICBtYWtlQmlkaXJlY3Rpb25hbE1ldGhvZChkZXNjcmlwdG9yLCB0eXBlKTtcblxuICAgICAgaWYgKGRlc2NyaXB0b3IudGVsZW1ldHJ5KVxuICAgICAgICBtZXRob2QgPSBwcm9maWxlcihtZXRob2QpO1xuICAgICAgaWYgKGRlc2NyaXB0b3IucmVsZWFzZSlcbiAgICAgICAgbWV0aG9kID0gZGVzdHJ1Y3RvcihtZXRob2QpO1xuXG4gICAgICByZXR1cm4gbWV0aG9kO1xuICAgIH1cblxuICAgIHZhciBtYWtlVW5pZGlyZWNhdGlvbmFsTWV0aG9kID0gZnVuY3Rpb24oZGVzY3JpcHRvciwgdHlwZSkge1xuICAgICAgcmV0dXJuIGZ1bmN0aW9uKCkge1xuICAgICAgICB2YXIgcGFja2V0ID0gdHlwZS53cml0ZShhcmd1bWVudHMsIHRoaXMpO1xuICAgICAgICBwYWNrZXQudG8gPSB0aGlzLmlkO1xuICAgICAgICBjbGllbnQuc2VuZChwYWNrZXQpO1xuICAgICAgICByZXR1cm4gUHJvbWlzZS5yZXNvbHZlKHZvaWQoMCkpO1xuICAgICAgfTtcbiAgICB9O1xuXG4gICAgdmFyIG1ha2VCaWRpcmVjdGlvbmFsTWV0aG9kID0gZnVuY3Rpb24oZGVzY3JpcHRvciwgdHlwZSkge1xuICAgICAgcmV0dXJuIGZ1bmN0aW9uKCkge1xuICAgICAgICB2YXIgY29udGV4dCA9IHRoaXMuY29udGV4dDtcbiAgICAgICAgdmFyIHBhY2tldCA9IHR5cGUud3JpdGUoYXJndW1lbnRzLCBjb250ZXh0KTtcbiAgICAgICAgdmFyIGNvbnRleHQgPSB0aGlzLmNvbnRleHQ7XG4gICAgICAgIHBhY2tldC50byA9IHRoaXMuaWQ7XG4gICAgICAgIHJldHVybiBjbGllbnQucmVxdWVzdChwYWNrZXQpLnRoZW4oZnVuY3Rpb24ocGFja2V0KSB7XG4gICAgICAgICAgcmV0dXJuIHR5cGUucmVhZChwYWNrZXQsIGNvbnRleHQpO1xuICAgICAgICB9KTtcbiAgICAgIH07XG4gICAgfTtcblxuICAgIHZhciBFdmVudCA9IENsYXNzKHtcbiAgICAgIGNvbnN0cnVjdG9yOiBmdW5jdGlvbihuYW1lLCBkZXNjcmlwdG9yKSB7XG4gICAgICAgIHRoaXMubmFtZSA9IGRlc2NyaXB0b3IudHlwZSB8fCBuYW1lO1xuICAgICAgICB0aGlzLmV2ZW50VHlwZSA9IGRlc2NyaXB0b3IudHlwZSB8fCBuYW1lO1xuICAgICAgICB0aGlzLnR5cGVzID0gT2JqZWN0LmNyZWF0ZShudWxsKTtcblxuICAgICAgICB2YXIgdHlwZXMgPSB0aGlzLnR5cGVzO1xuICAgICAgICBmb3IgKHZhciBrZXkgaW4gZGVzY3JpcHRvcikge1xuICAgICAgICAgIGlmIChrZXkgPT09IFwidHlwZVwiKSB7XG4gICAgICAgICAgICB0eXBlc1trZXldID0gXCJzdHJpbmdcIjtcbiAgICAgICAgICB9IGVsc2Uge1xuICAgICAgICAgICAgdHlwZXNba2V5XSA9IGRlc2NyaXB0b3Jba2V5XS50eXBlO1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgfSxcbiAgICAgIHJlYWQ6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHZhciBvdXRwdXQgPSB7fTtcbiAgICAgICAgdmFyIHR5cGVzID0gdGhpcy50eXBlcztcbiAgICAgICAgZm9yICh2YXIga2V5IGluIGlucHV0KSB7XG4gICAgICAgICAgb3V0cHV0W2tleV0gPSByZWFkKGlucHV0W2tleV0sIGNvbnRleHQsIHR5cGVzW2tleV0pO1xuICAgICAgICB9XG4gICAgICAgIHJldHVybiBvdXRwdXQ7XG4gICAgICB9LFxuICAgICAgd3JpdGU6IGZ1bmN0aW9uKGlucHV0LCBjb250ZXh0KSB7XG4gICAgICAgIHZhciBvdXRwdXQgPSB7fTtcbiAgICAgICAgdmFyIHR5cGVzID0gdGhpcy50eXBlcztcbiAgICAgICAgZm9yICh2YXIga2V5IGluIHRoaXMudHlwZXMpIHtcbiAgICAgICAgICBvdXRwdXRba2V5XSA9IHdyaXRlKGlucHV0W2tleV0sIGNvbnRleHQsIHR5cGVzW2tleV0pO1xuICAgICAgICB9XG4gICAgICAgIHJldHVybiBvdXRwdXQ7XG4gICAgICB9XG4gICAgfSk7XG5cbiAgICB2YXIgRnJvbnQgPSBDbGFzcyh7XG4gICAgICBleHRlbmRzOiBFdmVudFRhcmdldCxcbiAgICAgIEV2ZW50VGFyZ2V0OiBFdmVudFRhcmdldCxcbiAgICAgIGNvbnN0cnVjdG9yOiBmdW5jdGlvbihzdGF0ZSkge1xuICAgICAgICB0aGlzLkV2ZW50VGFyZ2V0KCk7XG4gICAgICAgIE9iamVjdC5kZWZpbmVQcm9wZXJ0aWVzKHRoaXMsICB7XG4gICAgICAgICAgc3RhdGU6IHtcbiAgICAgICAgICAgIGVudW1lcmFibGU6IGZhbHNlLFxuICAgICAgICAgICAgd3JpdGFibGU6IHRydWUsXG4gICAgICAgICAgICBjb25maWd1cmFibGU6IHRydWUsXG4gICAgICAgICAgICB2YWx1ZTogc3RhdGVcbiAgICAgICAgICB9XG4gICAgICAgIH0pO1xuXG4gICAgICAgIGNsaWVudC5yZWdpc3Rlcih0aGlzKTtcbiAgICAgIH0sXG4gICAgICBnZXQgaWQoKSB7XG4gICAgICAgIHJldHVybiB0aGlzLnN0YXRlLmFjdG9yO1xuICAgICAgfSxcbiAgICAgIGdldCBjb250ZXh0KCkge1xuICAgICAgICByZXR1cm4gdGhpcztcbiAgICAgIH0sXG4gICAgICBmb3JtOiBmdW5jdGlvbihzdGF0ZSwgZGV0YWlsLCBjb250ZXh0KSB7XG4gICAgICAgIGlmICh0aGlzLnN0YXRlICE9PSBzdGF0ZSkge1xuICAgICAgICAgIGlmIChkZXRhaWwpIHtcbiAgICAgICAgICAgIHRoaXMuc3RhdGVbZGV0YWlsXSA9IHN0YXRlW2RldGFpbF07XG4gICAgICAgICAgfSBlbHNlIHtcbiAgICAgICAgICAgIHBhaXJzKHN0YXRlKS5mb3JFYWNoKGZ1bmN0aW9uKHBhaXIpIHtcbiAgICAgICAgICAgICAgdmFyIGtleSA9IHBhaXJbMF0sIHZhbHVlID0gcGFpclsxXTtcbiAgICAgICAgICAgICAgdGhpcy5zdGF0ZVtrZXldID0gdmFsdWU7XG4gICAgICAgICAgICB9LCB0aGlzKTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cblxuICAgICAgICBpZiAoY29udGV4dCkge1xuICAgICAgICAgIGNsaWVudC5zdXBlcnZpc2UoY29udGV4dCwgdGhpcyk7XG4gICAgICAgIH1cbiAgICAgIH0sXG4gICAgICByZXF1ZXN0VHlwZXM6IGZ1bmN0aW9uKCkge1xuICAgICAgICByZXR1cm4gY2xpZW50LnJlcXVlc3Qoe1xuICAgICAgICAgIHRvOiB0aGlzLmlkLFxuICAgICAgICAgIHR5cGU6IFwicmVxdWVzdFR5cGVzXCJcbiAgICAgICAgfSkudGhlbihmdW5jdGlvbihwYWNrZXQpIHtcbiAgICAgICAgICByZXR1cm4gcGFja2V0LnJlcXVlc3RUeXBlcztcbiAgICAgICAgfSk7XG4gICAgICB9XG4gICAgfSk7XG4gICAgdHlwZXMucHJpbWl0aXZlID0gbmV3IFByaW1pdHZlKFwicHJpbWl0aXZlXCIpO1xuICAgIHR5cGVzLnN0cmluZyA9IG5ldyBQcmltaXR2ZShcInN0cmluZ1wiKTtcbiAgICB0eXBlcy5udW1iZXIgPSBuZXcgUHJpbWl0dmUoXCJudW1iZXJcIik7XG4gICAgdHlwZXMuYm9vbGVhbiA9IG5ldyBQcmltaXR2ZShcImJvb2xlYW5cIik7XG4gICAgdHlwZXMuanNvbiA9IG5ldyBQcmltaXR2ZShcImpzb25cIik7XG4gICAgdHlwZXMuYXJyYXkgPSBuZXcgUHJpbWl0dmUoXCJhcnJheVwiKTtcbiAgfSxcbiAgcmVnaXN0ZXJUeXBlczogZnVuY3Rpb24oZGVzY3JpcHRvcikge1xuICAgIHZhciBzcGVjaWZpY2F0aW9uID0gdGhpcy5zcGVjaWZpY2F0aW9uO1xuICAgIHZhbHVlcyhkZXNjcmlwdG9yLnR5cGVzKS5mb3JFYWNoKGZ1bmN0aW9uKGRlc2NyaXB0b3IpIHtcbiAgICAgIHNwZWNpZmljYXRpb25bZGVzY3JpcHRvci50eXBlTmFtZV0gPSBkZXNjcmlwdG9yO1xuICAgIH0pO1xuICB9XG59KTtcbmV4cG9ydHMuVHlwZVN5c3RlbSA9IFR5cGVTeXN0ZW07XG4iLCJcInVzZSBzdHJpY3RcIjtcblxudmFyIGtleXMgPSBPYmplY3Qua2V5cztcbmV4cG9ydHMua2V5cyA9IGtleXM7XG5cbi8vIFJldHVybnMgYXJyYXkgb2YgdmFsdWVzIGZvciB0aGUgZ2l2ZW4gb2JqZWN0LlxudmFyIHZhbHVlcyA9IGZ1bmN0aW9uKG9iamVjdCkge1xuICByZXR1cm4ga2V5cyhvYmplY3QpLm1hcChmdW5jdGlvbihrZXkpIHtcbiAgICByZXR1cm4gb2JqZWN0W2tleV1cbiAgfSk7XG59O1xuZXhwb3J0cy52YWx1ZXMgPSB2YWx1ZXM7XG5cbi8vIFJldHVybnMgW2tleSwgdmFsdWVdIHBhaXJzIGZvciB0aGUgZ2l2ZW4gb2JqZWN0LlxudmFyIHBhaXJzID0gZnVuY3Rpb24ob2JqZWN0KSB7XG4gIHJldHVybiBrZXlzKG9iamVjdCkubWFwKGZ1bmN0aW9uKGtleSkge1xuICAgIHJldHVybiBba2V5LCBvYmplY3Rba2V5XV1cbiAgfSk7XG59O1xuZXhwb3J0cy5wYWlycyA9IHBhaXJzO1xuXG5cbi8vIFF1ZXJpZXMgYW4gb2JqZWN0IGZvciB0aGUgZmllbGQgbmVzdGVkIHdpdGggaW4gaXQuXG52YXIgcXVlcnkgPSBmdW5jdGlvbihvYmplY3QsIHBhdGgpIHtcbiAgcmV0dXJuIHBhdGgucmVkdWNlKGZ1bmN0aW9uKG9iamVjdCwgZW50cnkpIHtcbiAgICByZXR1cm4gb2JqZWN0ICYmIG9iamVjdFtlbnRyeV1cbiAgfSwgb2JqZWN0KTtcbn07XG5leHBvcnRzLnF1ZXJ5ID0gcXVlcnk7XG5cbnZhciBpc09iamVjdCA9IGZ1bmN0aW9uKHgpIHtcbiAgcmV0dXJuIHggJiYgdHlwZW9mKHgpID09PSBcIm9iamVjdFwiXG59XG5cbnZhciBmaW5kUGF0aCA9IGZ1bmN0aW9uKG9iamVjdCwga2V5KSB7XG4gIHZhciBwYXRoID0gdm9pZCgwKTtcbiAgaWYgKG9iamVjdCAmJiB0eXBlb2Yob2JqZWN0KSA9PT0gXCJvYmplY3RcIikge1xuICAgIHZhciBuYW1lcyA9IGtleXMob2JqZWN0KTtcbiAgICBpZiAobmFtZXMuaW5kZXhPZihrZXkpID49IDApIHtcbiAgICAgIHBhdGggPSBbXTtcbiAgICB9IGVsc2Uge1xuICAgICAgdmFyIGluZGV4ID0gMDtcbiAgICAgIHZhciBjb3VudCA9IG5hbWVzLmxlbmd0aDtcbiAgICAgIHdoaWxlIChpbmRleCA8IGNvdW50ICYmICFwYXRoKXtcbiAgICAgICAgdmFyIGhlYWQgPSBuYW1lc1tpbmRleF07XG4gICAgICAgIHZhciB0YWlsID0gZmluZFBhdGgob2JqZWN0W2hlYWRdLCBrZXkpO1xuICAgICAgICBwYXRoID0gdGFpbCA/IFtoZWFkXS5jb25jYXQodGFpbCkgOiB0YWlsO1xuICAgICAgICBpbmRleCA9IGluZGV4ICsgMVxuICAgICAgfVxuICAgIH1cbiAgfVxuICByZXR1cm4gcGF0aDtcbn07XG5leHBvcnRzLmZpbmRQYXRoID0gZmluZFBhdGg7XG4iXX0=
(1)
});
