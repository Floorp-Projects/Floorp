gdb.execute('set print address on')

run_fragment('JSObject.null')

assert_pretty('null', '0x0')

