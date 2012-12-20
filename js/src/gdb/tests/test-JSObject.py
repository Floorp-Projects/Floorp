# Printing JSObjects.

assert_subprinter_registered('SpiderMonkey', 'ptr-to-JSObject')
run_fragment('JSObject.simple')

# These patterns look a little strange because of prolog.py's 'set print
# address off', which avoids putting varying addresses in the output. After
# the '(JSObject *) ', there is a 'void *' value printing as the empty
# string.

assert_pretty('glob', '(JSObject *)  [object global] delegate')
assert_pretty('plain', '(JSObject *)  [object Object]')
assert_pretty('func', '(JSObject *)  [object Function "dys"]')
assert_pretty('anon', '(JSObject *)  [object Function <unnamed>]')
assert_pretty('funcPtr', '(JSFunction *)  [object Function "formFollows"]')
