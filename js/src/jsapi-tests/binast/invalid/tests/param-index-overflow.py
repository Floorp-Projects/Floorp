def filter_ast(ast):
    # AssertedPositionalParameterName.index >= ARGNO_LIMIT - 1.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    func_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(func_stmt, 'EagerFunctionDeclaration')

    func_contents = utils.get_field(func_stmt, 'contents')
    utils.assert_interface(func_contents, 'FunctionOrMethodContents')

    param_scope = utils.get_field(func_contents, 'parameterScope')
    utils.assert_interface(param_scope, 'AssertedParameterScope')
    param_names = utils.get_field(param_scope, 'paramNames')

    param_name = utils.get_element(param_names, 0)
    utils.assert_interface(param_name, 'AssertedPositionalParameterName')

    index = utils.get_field(param_name, "index")
    utils.set_unsigned_long(index, 65536)

    return ast
