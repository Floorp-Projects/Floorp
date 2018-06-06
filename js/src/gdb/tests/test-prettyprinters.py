# Ignore flake8 errors "undefined name 'assert_pretty'"
# As it caused by the way we instanciate this file
# flake8: noqa: F821

import mozilla.prettyprinters

run_fragment('prettyprinters.implemented_types')


def implemented_type_names(expr):
    v = gdb.parse_and_eval(expr)
    it = mozilla.prettyprinters.implemented_types(v.type)
    return [str(_) for _ in it]


assert_eq(implemented_type_names('i'), ['int'])
assert_eq(implemented_type_names('a'), ['A', 'int'])
assert_eq(implemented_type_names('b'), ['B', 'A', 'int'])
assert_eq(implemented_type_names('c'), ['C'])
assert_eq(implemented_type_names('c_'), ['C_', 'C'])
assert_eq(implemented_type_names('e'), ['E', 'C', 'D'])
assert_eq(implemented_type_names('e_'), ['E_', 'E', 'C', 'D'])
assert_eq(implemented_type_names('f'), ['F', 'C', 'D'])
assert_eq(implemented_type_names('h'), ['H', 'F', 'G', 'C', 'D'])

# Check that our pretty-printers aren't interfering with printing other types.
assert_pretty('10', '10')
assert_pretty('(void*) 0', '')  # Because of 'set print address off'
