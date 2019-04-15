/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const protocol = require("devtools/shared/protocol");
const { propertyIteratorSpec } = require("devtools/shared/specs/property-iterator");
loader.lazyRequireGetter(this, "ChromeUtils");
loader.lazyRequireGetter(this, "ObjectUtils", "devtools/server/actors/object/utils");

/**
 * Creates an actor to iterate over an object's property names and values.
 *
 * @param objectActor ObjectActor
 *        The object actor.
 * @param options Object
 *        A dictionary object with various boolean attributes:
 *        - enumEntries Boolean
 *          If true, enumerates the entries of a Map or Set object
 *          instead of enumerating properties.
 *        - ignoreIndexedProperties Boolean
 *          If true, filters out Array items.
 *          e.g. properties names between `0` and `object.length`.
 *        - ignoreNonIndexedProperties Boolean
 *          If true, filters out items that aren't array items
 *          e.g. properties names that are not a number between `0`
 *          and `object.length`.
 *        - sort Boolean
 *          If true, the iterator will sort the properties by name
 *          before dispatching them.
 *        - query String
 *          If non-empty, will filter the properties by names and values
 *          containing this query string. The match is not case-sensitive.
 *          Regarding value filtering it just compare to the stringification
 *          of the property value.
 */
const PropertyIteratorActor  = protocol.ActorClassWithSpec(propertyIteratorSpec, {
  initialize(objectActor, options, conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    if (!DevToolsUtils.isSafeDebuggerObject(objectActor.obj)) {
      this.iterator = {
        size: 0,
        propertyName: index => undefined,
        propertyDescription: index => undefined,
      };
    } else if (options.enumEntries) {
      const cls = objectActor.obj.class;
      if (cls == "Map") {
        this.iterator = enumMapEntries(objectActor);
      } else if (cls == "WeakMap") {
        this.iterator = enumWeakMapEntries(objectActor);
      } else if (cls == "Set") {
        this.iterator = enumSetEntries(objectActor);
      } else if (cls == "WeakSet") {
        this.iterator = enumWeakSetEntries(objectActor);
      } else if (cls == "Storage") {
        this.iterator = enumStorageEntries(objectActor);
      } else {
        throw new Error("Unsupported class to enumerate entries from: " + cls);
      }
    } else if (
      ObjectUtils.isArray(objectActor.obj)
      && options.ignoreNonIndexedProperties
      && !options.query
    ) {
      this.iterator = enumArrayProperties(objectActor, options);
    } else {
      this.iterator = enumObjectProperties(objectActor, options);
    }
  },

  form() {
    return {
      type: this.typeName,
      actor: this.actorID,
      count: this.iterator.size,
    };
  },

  names({ indexes }) {
    const list = [];
    for (const idx of indexes) {
      list.push(this.iterator.propertyName(idx));
    }
    return {
      names: indexes,
    };
  },

  slice({ start, count }) {
    const ownProperties = Object.create(null);
    for (let i = start, m = start + count; i < m; i++) {
      const name = this.iterator.propertyName(i);
      ownProperties[name] = this.iterator.propertyDescription(i);
    }

    return {
      ownProperties,
    };
  },

  all() {
    return this.slice({ start: 0, count: this.iterator.size });
  },
});

/**
 * Helper function to create a grip from a Map/Set entry
 */
function gripFromEntry({ obj, hooks }, entry) {
  if (!isWorker) {
    entry = Cu.unwaiveXrays(entry);
  }
  return hooks.createValueGrip(
    ObjectUtils.makeDebuggeeValueIfNeeded(obj, entry));
}

function enumArrayProperties(objectActor, options) {
  return {
    size: ObjectUtils.getArrayLength(objectActor.obj),
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      return objectActor._propertyDescriptor(index);
    },
  };
}

function enumObjectProperties(objectActor, options) {
  let names = [];
  try {
    names = objectActor.obj.getOwnPropertyNames();
  } catch (ex) {
    // Calling getOwnPropertyNames() on some wrapped native prototypes is not
    // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
  }

  if (options.ignoreNonIndexedProperties || options.ignoreIndexedProperties) {
    const length = DevToolsUtils.getProperty(objectActor.obj, "length");
    let sliceIndex;

    const isLengthTrustworthy = isUint32(length)
      && (!length || ObjectUtils.isArrayIndex(names[length - 1]))
      && !ObjectUtils.isArrayIndex(names[length]);

    if (!isLengthTrustworthy) {
      // The length property may not reflect what the object looks like, let's find
      // where indexed properties end.

      if (!ObjectUtils.isArrayIndex(names[0])) {
        // If the first item is not a number, this means there is no indexed properties
        // in this object.
        sliceIndex = 0;
      } else {
        sliceIndex = names.length;
        while (sliceIndex > 0) {
          if (ObjectUtils.isArrayIndex(names[sliceIndex - 1])) {
            break;
          }
          sliceIndex--;
        }
      }
    } else {
      sliceIndex = length;
    }

    // It appears that getOwnPropertyNames always returns indexed properties
    // first, so we can safely slice `names` for/against indexed properties.
    // We do such clever operation to optimize very large array inspection.
    if (options.ignoreIndexedProperties) {
      // Keep items after `sliceIndex` index
      names = names.slice(sliceIndex);
    } else if (options.ignoreNonIndexedProperties) {
      // Keep `sliceIndex` first items
      names.length = sliceIndex;
    }
  }

  const safeGetterValues = objectActor._findSafeGetterValues(names, 0);
  const safeGetterNames = Object.keys(safeGetterValues);
  // Merge the safe getter values into the existing properties list.
  for (const name of safeGetterNames) {
    if (!names.includes(name)) {
      names.push(name);
    }
  }

  if (options.query) {
    let { query } = options;
    query = query.toLowerCase();
    names = names.filter(name => {
      // Filter on attribute names
      if (name.toLowerCase().includes(query)) {
        return true;
      }
      // and then on attribute values
      let desc;
      try {
        desc = objectActor.obj.getOwnPropertyDescriptor(name);
      } catch (e) {
        // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
        // allowed (bug 560072).
      }
      if (desc && desc.value &&
          String(desc.value).includes(query)) {
        return true;
      }
      return false;
    });
  }

  if (options.sort) {
    names.sort();
  }

  return {
    size: names.length,
    propertyName(index) {
      return names[index];
    },
    propertyDescription(index) {
      const name = names[index];
      let desc = objectActor._propertyDescriptor(name);
      if (!desc) {
        desc = safeGetterValues[name];
      } else if (name in safeGetterValues) {
        // Merge the safe getter values into the existing properties list.
        const { getterValue, getterPrototypeLevel } = safeGetterValues[name];
        desc.getterValue = getterValue;
        desc.getterPrototypeLevel = getterPrototypeLevel;
      }
      return desc;
    },
  };
}

function enumMapEntries(objectActor) {
  // Iterating over a Map via .entries goes through various intermediate
  // objects - an Iterator object, then a 2-element Array object, then the
  // actual values we care about. We don't have Xrays to Iterator objects,
  // so we get Opaque wrappers for them. And even though we have Xrays to
  // Arrays, the semantics often deny access to the entires based on the
  // nature of the values. So we need waive Xrays for the iterator object
  // and the tupes, and then re-apply them on the underlying values until
  // we fix bug 1023984.
  //
  // Even then though, we might want to continue waiving Xrays here for the
  // same reason we do so for Arrays above - this filtering behavior is likely
  // to be more confusing than beneficial in the case of Object previews.
  let keys, getValue;
  if (isWorker) {
    const keysIterator = DevToolsUtils.callPropertyOnObject(objectActor.obj, "keys");
    keys = [...DevToolsUtils.makeDebuggeeIterator(keysIterator)];
    const valuesIterator = DevToolsUtils.callPropertyOnObject(objectActor.obj, "values");
    const values = [...DevToolsUtils.makeDebuggeeIterator(valuesIterator)];
    const map = new Map();
    for (let i = 0; i < keys.length; i++) {
      map.set(keys[i], values[i]);
    }
    getValue = key => map.get(key);
  } else {
    const raw = objectActor.obj.unsafeDereference();
    keys = [...Cu.waiveXrays(Map.prototype.keys.call(raw))];
    getValue = key => Map.prototype.get.call(raw, key);
  }

  return {
    [Symbol.iterator]: function* () {
      for (const key of keys) {
        const value = getValue(key);
        yield [ key, value ].map(val => gripFromEntry(objectActor, val));
      }
    },
    size: keys.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const key = keys[index];
      const val = getValue(key);
      return {
        enumerable: true,
        value: {
          type: "mapEntry",
          preview: {
            key: gripFromEntry(objectActor, key),
            value: gripFromEntry(objectActor, val),
          },
        },
      };
    },
  };
}

function enumStorageEntries(objectActor) {
  // Iterating over local / sessionStorage entries goes through various
  // intermediate objects - an Iterator object, then a 2-element Array object,
  // then the actual values we care about. We don't have Xrays to Iterator
  // objects, so we get Opaque wrappers for them.
  const raw = objectActor.obj.unsafeDereference();
  const keys = [];
  for (let i = 0; i < raw.length; i++) {
    keys.push(raw.key(i));
  }
  return {
    [Symbol.iterator]: function* () {
      for (const key of keys) {
        const value = raw.getItem(key);
        yield [ key, value ].map(val => gripFromEntry(objectActor, val));
      }
    },
    size: keys.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const key = keys[index];
      const val = raw.getItem(key);
      return {
        enumerable: true,
        value: {
          type: "storageEntry",
          preview: {
            key: gripFromEntry(objectActor, key),
            value: gripFromEntry(objectActor, val),
          },
        },
      };
    },
  };
}

function enumWeakMapEntries(objectActor) {
  // We currently lack XrayWrappers for WeakMap, so when we iterate over
  // the values, the temporary iterator objects get created in the target
  // compartment. However, we _do_ have Xrays to Object now, so we end up
  // Xraying those temporary objects, and filtering access to |it.value|
  // based on whether or not it's Xrayable and/or callable, which breaks
  // the for/of iteration.
  //
  // This code is designed to handle untrusted objects, so we can safely
  // waive Xrays on the iterable, and relying on the Debugger machinery to
  // make sure we handle the resulting objects carefully.
  const raw = objectActor.obj.unsafeDereference();
  const basekeys = ChromeUtils.nondeterministicGetWeakMapKeys(raw);
  const keys = isWorker ? basekeys : Cu.waiveXrays(basekeys);

  const values = [];
  if (isWorker) {
    for (const k of keys) {
      const nk = ObjectUtils.makeDebuggeeValueIfNeeded(objectActor.obj, k);
      const v = DevToolsUtils.callPropertyOnObject(objectActor.obj, "get", nk);
      values.push(ObjectUtils.unwrapDebuggeeValue(v));
    }
  } else {
    for (const k of keys) {
      values.push(WeakMap.prototype.get.call(raw, k));
    }
  }

  return {
    [Symbol.iterator]: function* () {
      for (let i = 0; i < keys.length; i++) {
        yield [ keys[i], values[i] ].map(val => gripFromEntry(objectActor, val));
      }
    },
    size: keys.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const key = keys[index];
      const val = values[index];
      return {
        enumerable: true,
        value: {
          type: "mapEntry",
          preview: {
            key: gripFromEntry(objectActor, key),
            value: gripFromEntry(objectActor, val),
          },
        },
      };
    },
  };
}

function enumSetEntries(objectActor) {
  // We currently lack XrayWrappers for Set, so when we iterate over
  // the values, the temporary iterator objects get created in the target
  // compartment. However, we _do_ have Xrays to Object now, so we end up
  // Xraying those temporary objects, and filtering access to |it.value|
  // based on whether or not it's Xrayable and/or callable, which breaks
  // the for/of iteration.
  //
  // This code is designed to handle untrusted objects, so we can safely
  // waive Xrays on the iterable, and relying on the Debugger machinery to
  // make sure we handle the resulting objects carefully.
  let values;
  if (isWorker) {
    const iterator = DevToolsUtils.callPropertyOnObject(objectActor.obj, "values");
    values = [...DevToolsUtils.makeDebuggeeIterator(iterator)];
  } else {
    const raw = objectActor.obj.unsafeDereference();
    values = [...Cu.waiveXrays(Set.prototype.values.call(raw))];
  }

  return {
    [Symbol.iterator]: function* () {
      for (const item of values) {
        yield gripFromEntry(objectActor, item);
      }
    },
    size: values.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const val = values[index];
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, val),
      };
    },
  };
}

function enumWeakSetEntries(objectActor) {
  // We currently lack XrayWrappers for WeakSet, so when we iterate over
  // the values, the temporary iterator objects get created in the target
  // compartment. However, we _do_ have Xrays to Object now, so we end up
  // Xraying those temporary objects, and filtering access to |it.value|
  // based on whether or not it's Xrayable and/or callable, which breaks
  // the for/of iteration.
  //
  // This code is designed to handle untrusted objects, so we can safely
  // waive Xrays on the iterable, and relying on the Debugger machinery to
  // make sure we handle the resulting objects carefully.
  const raw = objectActor.obj.unsafeDereference();
  const basekeys = ChromeUtils.nondeterministicGetWeakSetKeys(raw);
  const keys = isWorker ? basekeys : Cu.waiveXrays(basekeys);

  return {
    [Symbol.iterator]: function* () {
      for (const item of keys) {
        yield gripFromEntry(objectActor, item);
      }
    },
    size: keys.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const val = keys[index];
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, val),
      };
    },
  };
}

/**
 * Returns true if the parameter can be stored as a 32-bit unsigned integer.
 * If so, it will be suitable for use as the length of an array object.
 *
 * @param num Number
 *        The number to test.
 * @return Boolean
 */
function isUint32(num) {
  return num >>> 0 === num;
}

module.exports = {
  PropertyIteratorActor,
  enumMapEntries,
  enumSetEntries,
  enumWeakMapEntries,
  enumWeakSetEntries,
};
