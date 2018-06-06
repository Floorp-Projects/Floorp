# Printing JSStrings.
# flake8: noqa: F821

assert_subprinter_registered('SpiderMonkey', 'ptr-to-JSString')
run_fragment('JSString.simple')

assert_pretty('empty', '""')
assert_pretty('x', '"x"')
assert_pretty('z', '"z"')
assert_pretty('xz', '"xz"')

stars = gdb.parse_and_eval('stars')
assert_eq(str(stars), "'*' <repeats 100 times>")

doubleStars = gdb.parse_and_eval('doubleStars')
assert_eq(str(doubleStars), "'*' <repeats 200 times>")

assert_pretty('xRaw', '"x"')

# JSAtom *

run_fragment('JSString.atom')

assert_pretty('molybdenum', '"molybdenum"')
