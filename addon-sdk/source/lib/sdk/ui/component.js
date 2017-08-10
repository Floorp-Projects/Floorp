/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Internal properties not exposed to the public.
const cache = Symbol("component/cache");
const writer = Symbol("component/writer");
const isFirstWrite = Symbol("component/writer/first-write?");
const currentState = Symbol("component/state/current");
const pendingState = Symbol("component/state/pending");
const isWriting = Symbol("component/writing?");

const isntNull = x => x !== null;

const Component = function(options, children) {
  this[currentState] = null;
  this[pendingState] = null;
  this[writer] = null;
  this[cache] = null;
  this[isFirstWrite] = true;

  this[Component.construct](options, children);
}
Component.Component = Component;
// Constructs component.
Component.construct = Symbol("component/construct");
// Called with `options` and `children` and must return
// initial state back.
Component.initial = Symbol("component/initial");

// Function patches current `state` with a given update.
Component.patch = Symbol("component/patch");
// Function that replaces current `state` with a passed state.
Component.reset = Symbol("component/reset");

// Function that must return render tree from passed state.
Component.render = Symbol("component/render");

// Path of the component with in the mount point.
Component.path = Symbol("component/path");

Component.isMounted = component => !!component[writer];
Component.isWriting = component => !!component[isWriting];

// Internal method that mounts component to a writer.
// Mounts component to a writer.
Component.mount = (component, write) => {
  if (Component.isMounted(component)) {
    throw Error("Can not mount already mounted component");
  }

  component[writer] = write;
  Component.write(component);

  if (component[Component.mounted]) {
    component[Component.mounted]();
  }
}

// Unmounts component from a writer.
Component.unmount = (component) => {
  if (Component.isMounted(component)) {
    component[writer] = null;
    if (component[Component.unmounted]) {
      component[Component.unmounted]();
    }
  } else {
    console.warn("Unmounting component that is not mounted is redundant");
  }
};
 // Method invoked once after inital write occurs.
Component.mounted = Symbol("component/mounted");
// Internal method that unmounts component from the writer.
Component.unmounted = Symbol("component/unmounted");
// Function that must return true if component is changed
Component.isUpdated = Symbol("component/updated?");
Component.update = Symbol("component/update");
Component.updated = Symbol("component/updated");

const writeChild = base => (child, index) => Component.write(child, base, index)
Component.write = (component, base, index) => {
  if (component === null) {
    return component;
  }

  if (!(component instanceof Component)) {
    const path = base ? `${base}${component.key || index}/` : `/`;
    return Object.assign({}, component, {
      [Component.path]: path,
      children: component.children && component.children.
                                        map(writeChild(path)).
                                        filter(isntNull)
    });
  }

  component[isWriting] = true;

  try {

    const current = component[currentState];
    const pending = component[pendingState] || current;
    const isUpdated = component[Component.isUpdated];
    const isInitial = component[isFirstWrite];

    if (isUpdated(current, pending) || isInitial) {
      if (!isInitial && component[Component.update]) {
        component[Component.update](pending, current)
      }

      // Note: [Component.update] could have caused more updates so can't use
      // `pending` as `component[pendingState]` may have changed.
      component[currentState] = component[pendingState] || current;
      component[pendingState] = null;

      const tree = component[Component.render](component[currentState]);
      component[cache] = Component.write(tree, base, index);
      if (component[writer]) {
        component[writer].call(null, component[cache]);
      }

      if (!isInitial && component[Component.updated]) {
        component[Component.updated](current, pending);
      }
    }

    component[isFirstWrite] = false;

    return component[cache];
  } finally {
    component[isWriting] = false;
  }
};

Component.prototype = Object.freeze({
  constructor: Component,

  [Component.mounted]: null,
  [Component.unmounted]: null,
  [Component.update]: null,
  [Component.updated]: null,

  get state() {
    return this[pendingState] || this[currentState];
  },


  [Component.construct](settings, items) {
    const initial = this[Component.initial];
    const base = initial(settings, items);
    const options = Object.assign(Object.create(null), base.options, settings);
    const children = base.children || items || null;
    const state = Object.assign(Object.create(null), base, {options, children});
    this[currentState] = state;

    if (this.setup) {
      this.setup(state);
    }
  },
  [Component.initial](options, children) {
    return Object.create(null);
  },
  [Component.patch](update) {
    this[Component.reset](Object.assign({}, this.state, update));
  },
  [Component.reset](state) {
    this[pendingState] = state;
    if (Component.isMounted(this) && !Component.isWriting(this)) {
      Component.write(this);
    }
  },

  [Component.isUpdated](before, after) {
    return before != after
  },

  [Component.render](state) {
    throw Error("Component must implement [Component.render] member");
  }
});

module.exports = Component;
