def filter_ast(ast):
    # AssertedDeclaredName with non-identifier string.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    scope = utils.get_field(ast, 'scope')
    utils.assert_interface(scope, 'AssertedScriptGlobalScope')

    decl_names = utils.get_field(scope, 'declaredNames')
    decl_name = utils.get_element(decl_names, 0)
    utils.assert_interface(decl_name, 'AssertedDeclaredName')

    copied_decl_name = utils.copy_tagged_tuple(decl_name)
    utils.append_element(decl_names, copied_decl_name)

    name = utils.get_field(copied_decl_name, 'name')

    utils.set_identifier_name(name, '1')

    return ast
