// |jit-test| skip-if: !('ctypes' in this)

gczeal(14, 1);
ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, []).ptr;
