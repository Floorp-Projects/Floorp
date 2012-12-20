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
