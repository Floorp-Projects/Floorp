<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Atul Varma [atul@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

<div class="warning">
<p>The <code>observer-service</code> module is deprecated.</p>
<p>To access the
<a href="https://developer.mozilla.org/en-US/docs/Observer_Notifications">observer service</a>, use the
<a href="modules/sdk/system/events.html"><code>system/events</code></a>
module.</p>
</div>

The `observer-service` module provides access to the
application-wide observer service singleton.

For a list of common observer topics across a variety of Mozilla-based
applications, see the MDC page on
[Observer Notifications](https://developer.mozilla.org/en/Observer_Notifications).

## Observer Callbacks ##

Observer callbacks are functions of the following form:

    function callback(subject, data) {
      /* Respond to the event notification here... */
    }

In the above example, `subject` is any JavaScript object, as is
`data`.  The particulars of what the two contain are specific
to the notification topic.

<api name="add">
@function
  Adds an observer callback to be triggered whenever a notification matching the
  topic is broadcast throughout the application.

@param topic {string}
  The topic to observe.

@param callback {function,object}
  Either a function or an object that implements [`nsIObserver`](http://mxr.mozilla.org/mozilla-central/source/xpcom/ds/nsIObserver.idl).
  If a function, then it is called when the notification occurs.  If an object,
  then its `observe()` method is called when the notification occurs.

@param [thisObject] {object}
  An optional object to use as `this` when a function callback is called.
</api>

<api name="remove">
@function
  Unsubscribes a callback from being triggered whenever a notification
  matching the topic is broadcast throughout the application.

@param topic {string}
  The topic being observed by the previous call to `add()`.

@param callback {function,object}
  The callback subscribed in the previous call to `add()`, either a function or
  object.

@param [thisObject] {object}
  If `thisObject` was passed to the previous call to `add()`, it should be
  passed to `remove()` as well.
</api>

<api name="notify">
@function
  Broadcasts a notification event for a topic, passing a subject and data to all
  applicable observers in the application.

@param topic {string}
  The topic about which to broadcast a notification.

@param [subject] {value}
  Optional information about the topic.  This can be any JS object or primitive.
  If you have multiple values to pass to observers, wrap them in an object,
  e.g., `{ foo: 1, bar: "some string", baz: myObject }`.
</api>
