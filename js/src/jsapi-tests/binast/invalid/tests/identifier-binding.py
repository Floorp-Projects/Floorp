def filter_ast(ast):
    # BindingIdentifier with non-identifier string.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    var_decl = utils.get_element(global_stmts, 0)
    utils.assert_interface(var_decl, 'VariableDeclaration')

    decls = utils.get_field(var_decl, 'declarators')

    decl = utils.get_element(decls, 0)
    utils.assert_interface(decl, 'VariableDeclarator')

    copied_decl = utils.copy_tagged_tuple(decl)
    utils.append_element(decls, copied_decl)

    binding = utils.get_field(copied_decl, 'binding')
    utils.assert_interface(binding, 'BindingIdentifier')

    name = utils.get_field(binding, 'name')

    utils.set_identifier_name(name, '1')

    return ast
