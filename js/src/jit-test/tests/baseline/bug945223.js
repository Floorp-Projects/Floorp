// |jit-test| error: InternalError

// This test case creates an array, with no third element. When we iterate over
// the third element of the array (GetElement) see that it is "undefined" and
// start looking for __noSuchMethod__. Iterating over the protoype chain end on
// the Proxy defined as the __proto__ of Array.
//
// When looking for the __noSuchMethod__ property (BaseProxyHandler::get) on the
// proxy, the function getPropertyDescriptor is called which cause an error.
//
// Unfortunately, IonMonkey GetElementIC do not have supports for
// __noSuchMethod__ (Bug 964574) at the moment, which cause a differential
// behaviour.
//
// As we hope to remote __noSuchMethod__ soon (Bug 683218), we disable the mode
// which force the selection of IonMonkey ICs.
if (getJitCompilerOptions()["ion.forceinlineCaches"])
    setJitCompilerOption("ion.forceinlineCaches", 0);

enableNoSuchMethod();

Array.prototype.__proto__ = Proxy.create({
    getPropertyDescriptor: function(name) {
	return (560566);
    },
}, null);
function f() {}
function g() {}
var x = [f,f,f,undefined,g];
for (var i = 0; i < 5; ++i)
  y = x[i]("x");
