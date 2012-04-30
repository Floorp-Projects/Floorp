// |reftest| skip
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jesse Ruderman <jruderman@gmail.com>

try {
  var x = Proxy.create( {get:function(r,name){return {}[name]}} );
  x.watch('e', function(){});
} catch (exc) {
}

reportCompare(0, 0, 'ok');
