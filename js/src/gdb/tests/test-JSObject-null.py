# flake8: noqa: F821

gdb.execute('set print address on')

run_fragment('JSObject.null')

assert_pretty('null', '0x0')
assert_pretty('nullRaw', '0x0')
