# We can print pointers to subclasses of JSString.

run_fragment('JSString.subclasses')

assert_pretty('flat', '"Hi!"')
