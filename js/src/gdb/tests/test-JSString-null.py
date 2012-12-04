gdb.execute('set print address on')

run_fragment('JSString.null')

assert_pretty('null', '0x0')
