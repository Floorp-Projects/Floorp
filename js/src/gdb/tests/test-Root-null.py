# Test printing roots that refer to NULL pointers.

# Since mozilla.prettyprinters.Pointer declines to create pretty-printers
# for null pointers, GDB built-in printing code ends up handling them. But
# as of 2012-11, GDB suppresses printing pointers in replacement values:
# see: http://sourceware.org/ml/gdb/2012-11/msg00055.html
#
# Thus, if the pretty-printer for js::Rooted simply returns the referent as
# a replacement value (which seems reasonable enough, if you want the
# pretty-printer to be completely transparent), and the referent is a null
# pointer, it prints as nothing at all.
#
# This test ensures that the js::Rooted pretty-printer doesn't make that
# mistake.

gdb.execute('set print address on')

run_fragment('Root.null')

assert_pretty('null', '0x0')
