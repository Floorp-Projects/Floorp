def filter_ast(ast):
    # Set different parameter name than one in scope.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    func_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(func_stmt, 'EagerFunctionDeclaration')

    func_contents = utils.get_field(func_stmt, 'contents')
    utils.assert_interface(func_contents, 'FunctionOrMethodContents')

    params = utils.get_field(func_contents, 'params')
    utils.assert_interface(params, 'FormalParameters')
    params_items = utils.get_field(params, 'items')

    param = utils.get_element(params_items, 0)
    utils.assert_interface(param, 'BindingIdentifier')

    name = utils.get_field(param, "name")
    utils.set_identifier_name(name, "b")

    return ast
