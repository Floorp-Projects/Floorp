def filter_ast(ast):
    # AssignmentTargetIdentifier with non-identifier string.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    fun_decl = utils.get_element(global_stmts, 0)
    utils.assert_interface(fun_decl, 'EagerFunctionDeclaration')

    binding = utils.get_field(fun_decl, 'name')
    utils.assert_interface(binding, 'BindingIdentifier')

    name = utils.get_field(binding, 'name')

    utils.set_identifier_name(name, '1')

    return ast
