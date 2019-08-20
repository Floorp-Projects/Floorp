/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const { Arg, RetVal, types } = protocol;

const childSpecs = {};

function createStorageSpec(options) {
  // common methods for all storage types
  const methods = {
    getStoreObjects: {
      request: {
        host: Arg(0),
        names: Arg(1, "nullable:array:string"),
        options: Arg(2, "nullable:json"),
      },
      response: RetVal(options.storeObjectType),
    },
    getFields: {
      request: {
        subType: Arg(0, "nullable:string"),
      },
      response: {
        value: RetVal("json"),
      },
    },
  };

  // extra methods specific for storage type
  Object.assign(methods, options.methods);

  childSpecs[options.typeName] = protocol.generateActorSpec({
    typeName: options.typeName,
    methods,
  });
}

// Cookies store object
types.addDictType("cookieobject", {
  uniqueKey: "string",
  name: "string",
  value: "longstring",
  path: "nullable:string",
  host: "string",
  hostOnly: "boolean",
  isSecure: "boolean",
  isHttpOnly: "boolean",
  creationTime: "number",
  lastAccessed: "number",
  expires: "number",
});

// Array of cookie store objects
types.addDictType("cookiestoreobject", {
  total: "number",
  offset: "number",
  data: "array:nullable:cookieobject",
});

// Common methods for edit/remove
const editRemoveMethods = {
  getFields: {
    request: {},
    response: {
      value: RetVal("json"),
    },
  },
  editItem: {
    request: {
      data: Arg(0, "json"),
    },
    response: {},
  },
  removeItem: {
    request: {
      host: Arg(0, "string"),
      name: Arg(1, "string"),
    },
    response: {},
  },
};

// Cookies actor spec
createStorageSpec({
  typeName: "cookies",
  storeObjectType: "cookiestoreobject",
  methods: Object.assign(
    {},
    editRemoveMethods,
    {
      addItem: {
        request: {
          guid: Arg(0, "string"),
        },
        response: {},
      },
    },
    {
      removeAll: {
        request: {
          host: Arg(0, "string"),
          domain: Arg(1, "nullable:string"),
        },
        response: {},
      },
    },
    {
      removeAllSessionCookies: {
        request: {
          host: Arg(0, "string"),
          domain: Arg(1, "nullable:string"),
        },
        response: {},
      },
    }
  ),
});

// Local Storage / Session Storage store object
types.addDictType("storageobject", {
  name: "string",
  value: "longstring",
});

// Common methods for local/session storage
const storageMethods = Object.assign(
  {},
  editRemoveMethods,
  {
    addItem: {
      request: {
        guid: Arg(0, "string"),
        host: Arg(1, "nullable:string"),
      },
      response: {},
    },
  },
  {
    removeAll: {
      request: {
        host: Arg(0, "string"),
      },
      response: {},
    },
  }
);

// Array of Local Storage / Session Storage store objects
types.addDictType("storagestoreobject", {
  total: "number",
  offset: "number",
  data: "array:nullable:storageobject",
});

createStorageSpec({
  typeName: "localStorage",
  storeObjectType: "storagestoreobject",
  methods: storageMethods,
});

createStorageSpec({
  typeName: "sessionStorage",
  storeObjectType: "storagestoreobject",
  methods: storageMethods,
});

types.addDictType("extensionobject", {
  name: "nullable:string",
  value: "nullable:longstring",
});

types.addDictType("extensionstoreobject", {
  total: "number",
  offset: "number",
  data: "array:nullable:extensionobject",
});

createStorageSpec({
  typeName: "extensionStorage",
  storeObjectType: "extensionstoreobject",
  methods: {},
});

types.addDictType("cacheobject", {
  url: "string",
  status: "string",
});

// Array of Cache store objects
types.addDictType("cachestoreobject", {
  total: "number",
  offset: "number",
  data: "array:nullable:cacheobject",
});

// Cache storage spec
createStorageSpec({
  typeName: "Cache",
  storeObjectType: "cachestoreobject",
  methods: {
    removeAll: {
      request: {
        host: Arg(0, "string"),
        name: Arg(1, "string"),
      },
      response: {},
    },
    removeItem: {
      request: {
        host: Arg(0, "string"),
        name: Arg(1, "string"),
      },
      response: {},
    },
  },
});

// Indexed DB store object
// This is a union on idb object, db metadata object and object store metadata
// object
types.addDictType("idbobject", {
  uniqueKey: "string",
  name: "nullable:string",
  db: "nullable:string",
  objectStore: "nullable:string",
  origin: "nullable:string",
  version: "nullable:number",
  storage: "nullable:string",
  objectStores: "nullable:number",
  keyPath: "nullable:string",
  autoIncrement: "nullable:boolean",
  indexes: "nullable:string",
  value: "nullable:longstring",
});

// Array of Indexed DB store objects
types.addDictType("idbstoreobject", {
  total: "number",
  offset: "number",
  data: "array:nullable:idbobject",
});

// Result of Indexed DB delete operation: can block or throw error
types.addDictType("idbdeleteresult", {
  blocked: "nullable:boolean",
  error: "nullable:string",
});

createStorageSpec({
  typeName: "indexedDB",
  storeObjectType: "idbstoreobject",
  methods: {
    removeDatabase: {
      request: {
        host: Arg(0, "string"),
        name: Arg(1, "string"),
      },
      response: RetVal("idbdeleteresult"),
    },
    removeAll: {
      request: {
        host: Arg(0, "string"),
        name: Arg(1, "string"),
      },
      response: {},
    },
    removeItem: {
      request: {
        host: Arg(0, "string"),
        name: Arg(1, "string"),
      },
      response: {},
    },
  },
});

// Update notification object
types.addDictType("storeUpdateObject", {
  changed: "nullable:json",
  deleted: "nullable:json",
  added: "nullable:json",
});

// Generate a type definition for an object with actors for all storage types.
types.addDictType(
  "storelist",
  Object.keys(childSpecs).reduce((obj, type) => {
    obj[type] = type;
    return obj;
  }, {})
);

exports.childSpecs = childSpecs;

exports.storageSpec = protocol.generateActorSpec({
  typeName: "storage",

  /**
   * List of event notifications that the server can send to the client.
   *
   *  - stores-update : When any store object in any storage type changes.
   *  - stores-cleared : When all the store objects are removed.
   *  - stores-reloaded : When all stores are reloaded. This generally mean that
   *                      we should refetch everything again.
   */
  events: {
    "stores-update": {
      type: "storesUpdate",
      data: Arg(0, "storeUpdateObject"),
    },
    "stores-cleared": {
      type: "storesCleared",
      data: Arg(0, "json"),
    },
    "stores-reloaded": {
      type: "storesReloaded",
      data: Arg(0, "json"),
    },
  },

  methods: {
    listStores: {
      request: {},
      response: RetVal("storelist"),
    },
  },
});
