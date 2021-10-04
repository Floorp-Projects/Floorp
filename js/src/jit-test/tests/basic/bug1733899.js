a = {}
a[Symbol.iterator] = function() {
  return {
    next() {
      return {
        done: this
      }
    }
  }
}
function b([[]] = a) {}
try {
  b();
} catch {}
