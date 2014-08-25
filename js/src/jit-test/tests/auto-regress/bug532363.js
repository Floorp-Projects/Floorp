// Binary: cache/js-dbg-32-410468c50ca1-linux
// Flags: -j
//
load(libdir + 'asserts.js');
assertThrowsInstanceOf(function() {
  for each(z in [0, 0, 0, 0]) { ({
      __parent__: []
    } = [])
  }
}, TypeError); // [].__parent__ is undefined
