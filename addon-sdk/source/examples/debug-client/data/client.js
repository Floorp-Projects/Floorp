/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
(function(exports) {
"use strict";


var describe = Object.getOwnPropertyDescriptor;
var Class = fields => {
  var constructor = fields.constructor || function() {};
  var ancestor = fields.extends || Object;



  var descriptor = {};
  for (var key of Object.keys(fields))
    descriptor[key] = describe(fields, key);

  var prototype = Object.create(ancestor.prototype, descriptor);

  constructor.prototype = prototype;
  prototype.constructor = constructor;

  return constructor;
};


var bus = function Bus() {
  var parser = new DOMParser();
  return parser.parseFromString("<EventTarget/>", "application/xml").documentElement;
}();

var GUID = new WeakMap();
GUID.id = 0;
var guid = x => GUID.get(x);
var setGUID = x => {
  GUID.set(x, ++ GUID.id);
};

var Emitter = Class({
  extends: EventTarget,
  constructor: function() {
   this.setupEmitter();
  },
  setupEmitter: function() {
    setGUID(this);
  },
  addEventListener: function(type, listener, capture) {
    bus.addEventListener(type + "@" + guid(this),
                         listener, capture);
  },
  removeEventListener: function(type, listener, capture) {
    bus.removeEventListener(type + "@" + guid(this),
                            listener, capture);
  }
});

function dispatch(target, type, data) {
  var event = new MessageEvent(type + "@" + guid(target), {
    bubbles: true,
    cancelable: false,
    data: data
  });
  bus.dispatchEvent(event);
}

var supervisedWorkers = new WeakMap();
var supervised = supervisor => {
  if (!supervisedWorkers.has(supervisor)) {
    supervisedWorkers.set(supervisor, new Map());
    supervisor.connection.addActorPool(supervisor);
  }
  return supervisedWorkers.get(supervisor);
};

var Supervisor = Class({
  extends: Emitter,
  constructor: function(...params) {
    this.setupEmitter(...params);
    this.setupSupervisor(...params);
  },
  Supervisor: function(connection) {
    this.connection = connection;
  },
  /**
   * Return the parent pool for this client.
   */
  supervisor: function() {
    return this.connection.poolFor(this.actorID);
  },
  /**
   * Override this if you want actors returned by this actor
   * to belong to a different actor by default.
   */
  marshallPool: function() { return this; },
    /**
   * Add an actor as a child of this pool.
   */
  supervise: function(actor) {
    if (!actor.actorID)
      actor.actorID = this.connection.allocID(actor.actorPrefix ||
                                              actor.typeName);

    supervised(this).set(actor.actorID, actor);
    return actor;
  },
  /**
   * Remove an actor as a child of this pool.
   */
  abandon: function(actor) {
    supervised(this).delete(actor.actorID);
  },
  // true if the given actor ID exists in the pool.
  has: function(actorID) {
    return supervised(this).has(actorID);
  },
  // Same as actor, should update debugger connection to use 'actor'
  // and then remove this.
  get: function(actorID) {
    return supervised(this).get(actorID);
  },
  actor: function(actorID) {
    return supervised(this).get(actorID);
  },
  isEmpty: function() {
    return supervised(this).size === 0;
  },
  /**
   * For getting along with the debugger server pools, should be removable
   * eventually.
   */
  cleanup: function() {
    this.destroy();
  },
  destroy: function() {
    var supervisor = this.supervisor();
    if (supervisor)
      supervisor.abandon(this);

    for (var actor of supervised(this).values()) {
      if (actor !== this) {
        var destroy = actor.destroy;
        // Disconnect destroy while we're destroying in case of (misbehaving)
        // circular ownership.
        if (destroy) {
          actor.destroy = null;
          destroy.call(actor);
          actor.destroy = destroy;
        }
      }
    }

    this.connection.removeActorPool(this);
    supervised(this).clear();
  }

});




var mailbox = new WeakMap();
var clientRequests = new WeakMap();

var inbox = client => mailbox.get(client).inbox;
var outbox = client => mailbox.get(client).outbox;
var requests = client => clientRequests.get(client);


var Receiver = Class({
  receive: function(packet) {
    if (packet.error)
      this.reject(packet.error);
    else
      this.resolve(this.read(packet));
  }
});

var Connection = Class({
  constructor: function() {
    // Queue of the outgoing messages.
    this.outbox = [];
    // Map of pending requests.
    this.pending = new Map();
    this.pools = new Set();
  },
  isConnected: function() {
    return !!this.port
  },
  connect: function(port) {
    this.port = port;
    port.addEventListener("message", this);
    port.start();

    this.flush();
  },
  addPool: function(pool) {
    this.pools.add(pool);
  },
  removePool: function(pool) {
    this.pools.delete(pool);
  },
  poolFor: function(id) {
    for (let pool of this.pools.values()) {
      if (pool.has(id))
        return pool;
    }
  },
  get: function(id) {
    var pool = this.poolFor(id);
    return pool && pool.get(id);
  },
  disconnect: function() {
    this.port.stop();
    this.port = null;
    for (var request of this.pending.values()) {
      request.catch(new Error("Connection closed"));
    }
    this.pending.clear();

    var requests = this.outbox.splice(0);
    for (var request of request) {
      requests.catch(new Error("Connection closed"));
    }
  },
  handleEvent: function(event) {
    this.receive(event.data);
  },
  flush: function() {
    if (this.isConnected()) {
      for (var request of this.outbox) {
        if (!this.pending.has(request.to)) {
          this.outbox.splice(this.outbox.indexOf(request), 1);
          this.pending.set(request.to, request);
          this.send(request.packet);
        }
      }
    }
  },
  send: function(packet) {
    this.port.postMessage(packet);
  },
  request: function(packet) {
    return new Promise(function(resolve, reject) {
      this.outbox.push({
        to: packet.to,
        packet: packet,
        receive: resolve,
        catch: reject
      });
      this.flush();
    });
  },
  receive: function(packet) {
    var { from, type, why } = packet;
    var receiver = this.pending.get(from);
    if (!receiver) {
      console.warn("Unable to handle received packet", data);
    } else {
      this.pending.delete(from);
      if (packet.error)
        receiver.catch(packet.error);
      else
        receiver.receive(packet);
    }
    this.flush();
  },
});

/**
 * Base class for client-side actor fronts.
 */
var Client = Class({
  extends: Supervisor,
  constructor: function(from=null, detail=null, connection=null) {
    this.Client(from, detail, connection);
  },
  Client: function(form, detail, connection) {
    this.Supervisor(connection);

    if (form) {
      this.actorID = form.actor;
      this.from(form, detail);
    }
  },
  connect: function(port) {
    this.connection = new Connection(port);
  },
  actorID: null,
  actor: function() {
    return this.actorID;
  },
  /**
   * Update the actor from its representation.
   * Subclasses should override this.
   */
  form: function(form) {
  },
  /**
   * Method is invokeid when packet received constitutes an
   * event. By default such packets are demarshalled and
   * dispatched on the client instance.
   */
  dispatch: function(packet) {
  },
  /**
   * Method is invoked when packet is returned in response to
   * a request. By default respond delivers response to a first
   * request in a queue.
   */
  read: function(input) {
    throw new TypeError("Subclass must implement read method");
  },
  write: function(input) {
    throw new TypeError("Subclass must implement write method");
  },
  respond: function(packet) {
    var [resolve, reject] = requests(this).shift();
    if (packet.error)
      reject(packet.error);
    else
      resolve(this.read(packet));
  },
  receive: function(packet) {
    if (this.isEventPacket(packet)) {
      this.dispatch(packet);
    }
    else if (requests(this).length) {
      this.respond(packet);
    }
    else {
      this.catch(packet);
    }
  },
  send: function(packet) {
    Promise.cast(packet.to || this.actor()).then(id => {
      packet.to = id;
      this.connection.send(packet);
    })
  },
  request: function(packet) {
    return this.connection.request(packet);
  }
});


var Destructor = method => {
  return function(...args) {
    return method.apply(this, args).then(result => {
      this.destroy();
      return result;
    });
  };
};

var Profiled = (method, id) => {
  return function(...args) {
    var start = new Date();
    return method.apply(this, args).then(result => {
      var end = new Date();
      this.telemetry.add(id, +end - start);
      return result;
    });
  };
};

var Method = (request, response) => {
  return response ? new BidirectionalMethod(request, response) :
         new UnidirecationalMethod(request);
};

var UnidirecationalMethod = request => {
  return function(...args) {
    var packet = request.write(args, this);
    this.connection.send(packet);
    return Promise.resolve(void(0));
  };
};

var BidirectionalMethod = (request, response) => {
  return function(...args) {
    var packet = request.write(args, this);
    return this.connection.request(packet).then(packet => {
      return response.read(packet, this);
    });
  };
};


Client.from = ({category, typeName, methods, events}) => {
  var proto = {
    constructor: function(...args) {
      this.Client(...args);
    },
    extends: Client,
    name: typeName
  };

  methods.forEach(({telemetry, request, response, name, oneway, release}) => {
    var [reader, writer] = oneway ? [, new Request(request)] :
                           [new Request(request), new Response(response)];
    var method = new Method(request, response);
    var profiler = telemetry ? new Profiler(method) : method;
    var destructor = release ? new Destructor(profiler) : profiler;
    proto[name] = destructor;
  });

  return Class(proto);
};


var defineType = (client, descriptor) => {
  var type = void(0)
  if (typeof(descriptor) === "string") {
    if (name.indexOf(":") > 0)
      type = makeCompoundType(descriptor);
    else if (name.indexOf("#") > 0)
      type = new ActorDetail(descriptor);
    else if (client.specification[descriptor])
      type = makeCategoryType(client.specification[descriptor]);
  } else {
    type = makeCategoryType(descriptor);
  }

  if (type)
    client.types.set(type.name, type);
  else
    throw TypeError("Invalid type: " + descriptor);
};


var makeCompoundType = name => {
  var index = name.indexOf(":");
  var [baseType, subType] = [name.slice(0, index), parts.slice(1)];
  return baseType === "array" ? new ArrayOf(subType) :
         baseType === "nullable" ? new Maybe(subType) :
         null;
};

var makeCategoryType = (descriptor) => {
  var { category } = descriptor;
  return category === "dict" ? new Dictionary(descriptor) :
         category === "actor" ? new Actor(descriptor) :
         null;
};


var typeFor = (client, type="primitive") => {
  if (!client.types.has(type))
    defineType(client, type);

  return client.types.get(type);
};


var Client = Class({
  constructor: function() {
  },
  setupTypes: function(specification) {
    this.specification = specification;
    this.types = new Map();
  },
  read: function(input, type) {
    return typeFor(this, type).read(input, this);
  },
  write: function(input, type) {
    return typeFor(this, type).write(input, this);
  }
});


var Type = Class({
  get name() {
    return this.category ? this.category + ":" + this.type :
           this.type;
  },
  read: function(input, client) {
    throw new TypeError("`Type` subclass must implement `read`");
  },
  write: function(input, client) {
    throw new TypeError("`Type` subclass must implement `write`");
  }
});


var Primitve = Class({
  extends: Type,
  constuctor: function(type) {
    this.type = type;
  },
  read: function(input, client) {
    return input;
  },
  write: function(input, client) {
    return input;
  }
});

var Maybe = Class({
  extends: Type,
  category: "nullable",
  constructor: function(type) {
    this.type = type;
  },
  read: function(input, client) {
    return input === null ? null :
           input === void(0) ? void(0) :
           client.read(input, this.type);
  },
  write: function(input, client) {
    return input === null ? null :
           input === void(0) ? void(0) :
           client.write(input, this.type);
  }
});

var ArrayOf = Class({
  extends: Type,
  category: "array",
  constructor: function(type) {
    this.type = type;
  },
  read: function(input, client) {
    return input.map($ => client.read($, this.type));
  },
  write: function(input, client) {
    return input.map($ => client.write($, this.type));
  }
});

var Dictionary = Class({
  exteds: Type,
  category: "dict",
  get name() { return this.type; },
  constructor: function({typeName, specializations}) {
    this.type = typeName;
    this.types = specifications;
  },
  read: function(input, client) {
    var output = {};
    for (var key in input) {
      output[key] = client.read(input[key], this.types[key]);
    }
    return output;
  },
  write: function(input, client) {
    var output = {};
    for (var key in input) {
      output[key] = client.write(value, this.types[key]);
    }
    return output;
  }
});

var Actor = Class({
  exteds: Type,
  category: "actor",
  get name() { return this.type; },
  constructor: function({typeName}) {
    this.type = typeName;
  },
  read: function(input, client, detail) {
    var id = value.actor;
    var actor = void(0);
    if (client.connection.has(id)) {
      return client.connection.get(id).form(input, detail, client);
    } else {
      actor = Client.from(detail, client);
      actor.actorID = id;
      client.supervise(actor);
    }
  },
  write: function(input, client, detail) {
    if (input instanceof Actor) {
      if (!input.actorID) {
        client.supervise(input);
      }
      return input.from(detail);
    }
    return input.actorID;
  }
});

var Root = Client.from({
  "category": "actor",
  "typeName": "root",
  "methods": [
    {"name": "listTabs",
     "request": {},
     "response": {
     }
    },
    {"name": "listAddons"
    },
    {"name": "echo",

    },
    {"name": "protocolDescription",

    }
  ]
});


var ActorDetail = Class({
  extends: Actor,
  constructor: function(name, actor, detail) {
    this.detail = detail;
    this.actor = actor;
  },
  read: function(input, client) {
    this.actor.read(input, client, this.detail);
  },
  write: function(input, client) {
    this.actor.write(input, client, this.detail);
  }
});

var registeredLifetimes = new Map();
var LifeTime = Class({
  extends: Type,
  category: "lifetime",
  constructor: function(lifetime, type) {
    this.name = lifetime + ":" + type.name;
    this.field = registeredLifetimes.get(lifetime);
  },
  read: function(input, client) {
    return this.type.read(input, client[this.field]);
  },
  write: function(input, client) {
    return this.type.write(input, client[this.field]);
  }
});

var primitive = new Primitve("primitive");
var string = new Primitve("string");
var number = new Primitve("number");
var boolean = new Primitve("boolean");
var json = new Primitve("json");
var array = new Primitve("array");


var TypedValue = Class({
  extends: Type,
  constructor: function(name, type) {
    this.TypedValue(name, type);
  },
  TypedValue: function(name, type) {
    this.name = name;
    this.type = type;
  },
  read: function(input, client) {
    return this.client.read(input, this.type);
  },
  write: function(input, client) {
    return this.client.write(input, this.type);
  }
});

var Return = Class({
  extends: TypedValue,
  constructor: function(type) {
    this.type = type
  }
});

var Argument = Class({
  extends: TypedValue,
  constructor: function(...args) {
    this.Argument(...args);
  },
  Argument: function(index, type) {
    this.index = index;
    this.TypedValue("argument[" + index + "]", type);
  },
  read: function(input, client, target) {
    return target[this.index] = client.read(input, this.type);
  }
});

var Option = Class({
  extends: Argument,
  constructor: function(...args) {
    return this.Argument(...args);
  },
  read: function(input, client, target, name) {
    var param = target[this.index] || (target[this.index] = {});
    param[name] = input === void(0) ? input : client.read(input, this.type);
  },
  write: function(input, client, name) {
    var value = input && input[name];
    return value === void(0) ? value : client.write(value, this.type);
  }
});

var Request = Class({
  extends: Type,
  constructor: function(template={}) {
    this.type = template.type;
    this.template = template;
    this.params = findPlaceholders(template, Argument);
  },
  read: function(packet, client) {
    var args = [];
    for (var param of this.params) {
      var {placeholder, path} = param;
      var name = path[path.length - 1];
      placeholder.read(getPath(packet, path), client, args, name);
      // TODO:
      // args[placeholder.index] = placeholder.read(query(packet, path), client);
    }
    return args;
  },
  write: function(input, client) {
    return JSON.parse(JSON.stringify(this.template, (key, value) => {
      return value instanceof Argument ? value.write(input[value.index],
                                                     client, key) :
             value;
    }));
  }
});

var Response = Class({
  extends: Type,
  constructor: function(template={}) {
    this.template = template;
    var [x] = findPlaceholders(template, Return);
    var {placeholder, path} = x;
    this.return = placeholder;
    this.path = path;
  },
  read: function(packet, client) {
    var value = query(packet, this.path);
    return this.return.read(value, client);
  },
  write: function(input, client) {
    return JSON.parse(JSON.stringify(this.template, (key, value) => {
      return value instanceof Return ? value.write(input) :
             input
    }));
  }
});

// Returns array of values for the given object.
var values = object => Object.keys(object).map(key => object[key]);
// Returns [key, value] pairs for the given object.
var pairs = object => Object.keys(object).map(key => [key, object[key]]);
// Queries an object for the field nested with in it.
var query = (object, path) => path.reduce((object, entry) => object && object[entry],
                                          object);


var Root = Client.from({
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
      "name": "actorDescriptions",
      "request": {},
      "response": { "_retval": "json" }
    }
  ],
  "events": {
    "tabListChanged": {}
  }
});

var Tab = Client.from({
  "category": "dict",
  "typeName": "tab",
  "specifications": {
    "title": "string",
    "url": "string",
    "outerWindowID": "number",
    "console": "console",
    "inspectorActor": "inspector",
    "callWatcherActor": "call-watcher",
    "canvasActor": "canvas",
    "webglActor": "webgl",
    "webaudioActor": "webaudio",
    "styleSheetsActor": "stylesheets",
    "styleEditorActor": "styleeditor",
    "storageActor": "storage",
    "gcliActor": "gcli",
    "memoryActor": "memory",
    "eventLoopLag": "eventLoopLag",

    "trace": "trace", // missing
  }
});

var tablist = Client.from({
  "category": "dict",
  "typeName": "tablist",
  "specializations": {
    "selected": "number",
    "tabs": "array:tab"
  }
});

})(this);

