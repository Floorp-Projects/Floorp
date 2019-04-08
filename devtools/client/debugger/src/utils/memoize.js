/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

type Value = any;
type Key = any;
type Store = WeakMap<Key, Store | Value>;

function hasValue(keys: Key[], store: Store) {
  let currentStore = store;
  for (const key of keys) {
    if (!currentStore || !currentStore.has(key)) {
      return false;
    }

    currentStore = currentStore.get(key);
  }
  return true;
}

function getValue(keys: Key[], store: Store): Value {
  let currentStore = store;
  for (const key of keys) {
    if (!currentStore) {
      return null;
    }
    currentStore = currentStore.get(key);
  }

  return currentStore;
}

function setValue(keys: Key[], store: Store, value: Value) {
  const keysExceptLast = keys.slice(0, -1);
  const lastKey = keys[keys.length - 1];

  let currentStore = store;
  for (const key of keysExceptLast) {
    if (!currentStore) {
      return;
    }

    if (!currentStore.has(key)) {
      currentStore.set(key, new WeakMap());
    }
    currentStore = currentStore.get(key);
  }

  if (currentStore) {
    currentStore.set(lastKey, value);
  }
}

// memoize with n arguments
export default function memoize(func: Function) {
  const store = new WeakMap();

  return function(...keys: Key[]) {
    if (hasValue(keys, store)) {
      return getValue(keys, store);
    }

    const newValue = func.apply(null, keys);
    setValue(keys, store, newValue);
    return newValue;
  };
}
