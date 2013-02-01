# Tests for jsid pretty-printing

assert_subprinter_registered('SpiderMonkey', 'jsid')

run_fragment('jsid.simple')

assert_pretty('string_id', '$jsid("moon")')
assert_pretty('int_id', '$jsid(1729)')
assert_pretty('void_id', 'JSID_VOID')
assert_pretty('object_id', '$jsid((JSObject *)  [object global] delegate)')
assert_pretty('xml_id', 'JS_DEFAULT_XML_NAMESPACE_ID')
