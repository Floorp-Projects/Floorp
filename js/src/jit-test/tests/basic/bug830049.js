// |jit-test| error: InternalError

p = Proxy.create({
  has: function() function r() s += ''
})
Object.prototype.__proto__ = p
function TestCase(n) {
    this.name = n
}
new TestCase()
