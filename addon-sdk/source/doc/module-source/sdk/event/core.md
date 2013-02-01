<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Many modules in the SDK can broadcast events. For example, the
[`tabs`](modules/sdk/tabs.html) module emits an `open` event when a new tab
is opened.

The `event/core` module enables you to create APIs that broadcast events.
Users of your API can listen to the events using the standard `on()` and
`once()` functions.

Also see the
[tutorial on implementing event targets](dev-guide/tutorials/event-targets.html)
to get started with this API.

An event `listener` may be registered to any event `target` using the
`on` function:

    var { on, once, off, emit } = require('sdk/event/core');
    var target = { name: 'target' };
    on(target, 'message', function listener(event) {
      console.log('hello ' + event);
    });
    on(target, 'data', console.log);

An event of a specific `type` may be emitted on any event `target`
object using the `emit` function. This will call all registered
`listener`s for the given `type` on the given event `target` in the
same order they were registered.

    emit(target, 'message', 'event');
    // info: 'hello event'
    emit(target, 'data', { type: 'data' }, 'second arg');
    // info: [Object object] 'second arg'

Registered event listeners may be removed using `off` function:

    off(target, 'message');
    emit(target, 'message', 'bye');
    // info: 'hello bye'

Sometimes listener only cares about first event of specific `type`. To avoid
hassles of removing such listeners there is a convenient `once` function:

    once(target, 'load', function() {
      console.log('ready');
    });
    emit(target, 'load')
    // info: 'ready'
    emit(target, 'load')

There are also convenient ways to remove registered listeners. All listeners of
the specific type can be easily removed (only two argument must be passed):

    off(target, 'message');

Also, removing all registered listeners is possible (only one argument must be
passed):

    off(target);

<api name="on">
@function
  Registers an event `listener` that is called every time events of
  the specified `type` is emitted on the given event `target`.

 @param target {Object}
    Event target object.
 @param type {String}
    The type of event.
 @param listener {Function}
    The listener function that processes the event.
</api>

<api name="once">
@function
  Registers an event `listener` that is called only once:
  the next time an event of the specified `type` is emitted
  on the given event `target`.

 @param target {Object}
    Event target object.
 @param type {String}
    The type of event.
 @param listener {Function}
    The listener function that processes the event.
</api>

<api name="emit">
@function
  Execute each of the listeners in order with the supplied arguments.
  All the exceptions that are thrown by listeners during the emit
  are caught and can be handled by listeners of 'error' event. Thrown
  exceptions are passed as an argument to an 'error' event listener.
  If no 'error' listener is registered exception will be logged into an
  error console.

  @param target {Object}
     Event target object.
  @param type {String}
     The type of event.
  @param message {Object|Number|String|Boolean}
     First argument that will be passed to listeners.
  @param arguments {Object|Number|String|Boolean}
     More arguments that will be passed to listeners.
</api>

<api name="off">
@function
  Removes an event `listener` for the given event `type` on the given event
  `target`. If no `listener` is passed removes all listeners of the given
  `type`. If `type` is not passed removes all the listeners of the given
  event `target`.

 @param target {Object}
    Event target object.
 @param type {String}
    The type of event.
 @param listener {Function}
    The listener function that processes the event.
</api>

<api name="count">
@function
  Returns a number of event listeners registered for the given event `type`
  on the given event `target`.
</api>