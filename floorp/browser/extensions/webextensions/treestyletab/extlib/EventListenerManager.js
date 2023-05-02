/*
 license: The MIT License, Copyright (c) 2018 YUKI "Piro" Hiroshi
 original:
   https://github.com/piroor/webextensions-lib-event-listener-manager
*/

'use strict';

const TIMEOUT = 2000;

export default class EventListenerManager {
  constructor() {
    this._listeners = new Set();
    this._stacksOnListenerAdded = new WeakMap();
    this._sourceOfListeners = new WeakMap();
  }

  addListener(listener) {
    const listeners = this._listeners;
    if (!listeners.has(listener)) {
      listeners.add(listener);
      if (EventListenerManager.debug) {
        const stack = new Error().stack;
        this._stacksOnListenerAdded.set(listener, stack);
        this._sourceOfListeners.set(listener, stack.split('\n')[1]);
      }
    }
  }

  removeListener(listener) {
    this._listeners.delete(listener);
    this._stacksOnListenerAdded.delete(listener);
    this._sourceOfListeners.delete(listener);
  }

  removeAllListeners() {
    this._listeners.clear();
    this._stacksOnListenerAdded.clear();
    this._sourceOfListeners.clear();
  }

  dispatch(...args) {
    const results = this.dispatchWithDetails(...args);
    if (results instanceof Promise)
      return results.then(this.normalizeResults);
    else
      return this.normalizeResults(results);
  }

  // We hope to process results synchronously if possibly,
  // so this method must not be "async".
  dispatchWithDetails(...args) {
    const listeners = Array.from(this._listeners);
    const results = listeners.map(listener => {
      const startAt = EventListenerManager.debug && Date.now();
      const timer = EventListenerManager.debug && setTimeout(() => {
        const listenerAddedStack = this._stacksOnListenerAdded.get(listener);
        console.log(`listener does not respond in ${TIMEOUT}ms.\n----------------------\n${listenerAddedStack || 'non debug mode or already removed listener:\n'+listener.toString()}\n----------------------\n${new Error().stack}\n----------------------\nargs:`, args);
      }, TIMEOUT);
      try {
        const result = listener(...args);
        if (result instanceof Promise)
          return result
            .catch(e => {
              console.log(e);
            })
            .then(result => {
              if (timer)
                clearTimeout(timer);
              return {
                value:    result,
                elapsed:  EventListenerManager.debug && (Date.now() - startAt),
                async:    true,
                listener: EventListenerManager.debug && this._sourceOfListeners.get(listener)
              };
            });
        if (timer)
          clearTimeout(timer);
        return {
          value:    result,
          elapsed:  EventListenerManager.debug && (Date.now() - startAt),
          async:    false,
          listener: EventListenerManager.debug && this._sourceOfListeners.get(listener)
        };
      }
      catch(e) {
        console.log(e);
        if (timer)
          clearTimeout(timer);
      }
    });
    if (results.some(result => result instanceof Promise))
      return Promise.all(results);
    else
      return results;
  }

  normalizeResults(results) {
    if (results.length == 1)
      return results[0].value;
    for (const result of results) {
      if (result.value === false)
        return false;
    }
    return true;
  }
}

EventListenerManager.debug = false;
