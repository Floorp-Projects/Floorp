try {
  __defineGetter__("eval", function() {
    this["__proto__"]
  })
  delete this["__proto__"]
  this["__proto__"]
} catch (e) {}
eval
