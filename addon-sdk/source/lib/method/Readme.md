# method

[![Build Status](https://secure.travis-ci.org/Gozala/method.png)](http://travis-ci.org/Gozala/method)

Library provides an API for defining polymorphic methods that dispatch on the
first argument type. This provides a powerful way for decouple abstraction
interface definition from an actual implementation per type, without risks
of interference with other libraries.

### Motivation

  - Provide a high-performance, dynamic polymorphism construct as an
    alternative to existing object methods that does not provides any
    mechanics for guarding against name conflicts.
  - Allow independent extension of types, and implementations of methods
    on types, by different parties.

## Install

    npm install method

## Use

```js
var method = require("method")

// Define `isWatchable` method that can be implemented for any type.
var isWatchable = method("isWatchable")

// If you call it on any object it will
// throw as nothing implements that method yet.
//isWatchable({}) // => Exception: method is not implemented

// If you define private method on `Object.prototype`
// all objects will inherit it.
Object.prototype[isWatchable] = function() {
  return false;
}

isWatchable({}) // => false


// Although `isWatchable` property above will be enumerable and there for
// may damage some assumbtions made by other libraries. There for it"s
// recomended to use built-in helpers methods that will define extension
// without breaking assumbtions made by other libraries:

isWatchable.define(Object, function() { return false })


// There are primitive types in JS that won"t inherit methods from Object:
isWatchable(null) // => Exception: method is not implemented

// One could either implement methods for such types:
isWatchable.define(null, function() { return false })
isWatchable.define(undefined, function() { return false })

// Or simply define default implementation:
isWatchable.define(function() { return false })

// Alternatively default implementation may be provided at creation:
isWatchable = method(function() { return false })

// Method dispatches on an first argument type. That allows us to create
// new types with an alternative implementations:
function Watchable() {}
isWatchable.define(Watchable, function() { return true })

// This will make all `Watchable` instances watchable!
isWatchable(new Watchable()) // => true

// Arbitrary objects can also be extended to implement given method. For example
// any object can simply made watchable:
function watchable(object) {
  return isWatchable.implement(objct, function() { return true })
}

isWatchable(watchable({})) // => true

// Full protocols can be defined with such methods:
var observers = "observers@" + module.filename
var watchers = method("watchers")
var watch = method("watch")
var unwatch = method("unwatch")

watchers.define(Watchable, function(target) {
  return target[observers] || (target[observers] = [])
})

watch.define(Watchable, function(target, watcher) {
  var observers = watchers(target)
  if (observers.indexOf(watcher) < 0) observers.push(watcher)
  return target
})
unwatch.define(Watchable, function(target, watcher) {
  var observers = watchers(target)
  var index = observers.indexOf(watcher)
  if (observers.indexOf(watcher) >= 0) observers.unshift(watcher)
  return target
})

// Define type Port that inherits form Watchable

function Port() {}
Port.prototype = Object.create(Watchable.prototype)

var emit = method("emit")
emit.define(Port, function(port, message) {
  watchers(port).slice().forEach(function(watcher) {
    watcher(message)
  })
})

var p = new Port()
watch(p, console.log)
emit(p, "hello world") // => info: "hello world"
```
