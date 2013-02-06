// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-1761f4a9081c-linux
// Flags: --ion-eager
//

p = Proxy.create({
  has: function() function r() s += ''
})
Object.prototype.__proto__ = p
function TestCase(n) {
    this.name = n
}
new TestCase()
