def filter_ast(ast):
    # Put WTF-8 string into scope name.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')

    scope = utils.get_field(ast, 'scope')
    utils.assert_interface(scope, 'AssertedScriptGlobalScope')

    names = utils.get_field(scope, 'declaredNames')

    decl_name = utils.get_element(names, 0)
    utils.assert_interface(decl_name, 'AssertedDeclaredName')

    name = utils.get_field(decl_name, 'name')
    utils.set_identifier_name(name, u'\uD83E_\uDD9D')

    return ast
