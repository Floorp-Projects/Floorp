const CTYPES_TEST_LIB = ctypes.libraryName("jsctypes-test");

registerReceiver("testCTypes", function(name, libdir) {
  var library = ctypes.open(libdir + '/' + CTYPES_TEST_LIB);
  let test_void_t = library.declare("test_void_t_cdecl", ctypes.default_abi, ctypes.void_t);
  sendMessage("onCTypesTested", test_void_t() === undefined);
});
