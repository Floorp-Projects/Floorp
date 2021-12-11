# Printing JSObjects.
# flake8: noqa: F821

assert_subprinter_registered("SpiderMonkey", "ptr-to-JSObject")
assert_subprinter_registered("SpiderMonkey", "ref-to-JSObject")

run_fragment("JSObject.simple")

# These patterns look a little strange because of prologue.py's 'set print
# address off', which avoids putting varying addresses in the output. After
# the '(JSObject *) ', there is a 'void *' value printing as the empty
# string.

assert_pretty("glob", "(JSObject *)  [object global]")
assert_pretty("plain", "(JSObject *)  [object Object]")
assert_pretty("objectProto", "(JSObject *)  [object Object] used_as_prototype")
assert_pretty("func", '(JSObject *)  [object Function "dys"]')
assert_pretty("anon", "(JSObject *)  [object Function <unnamed>]")
assert_pretty("funcPtr", '(JSFunction *)  [object Function "formFollows"]')

assert_pretty("badClassName", "(JSObject *)  [object \\307X]")

assert_pretty("plainRef", "(JSObject &) @ [object Object]")
assert_pretty("funcRef", '(JSFunction &) @ [object Function "formFollows"]')
