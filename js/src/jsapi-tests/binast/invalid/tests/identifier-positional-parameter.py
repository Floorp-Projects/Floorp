def filter_ast(ast):
    # AssertedPositionalParameterName with non-identifier string.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    fun_decl = utils.get_element(global_stmts, 0)
    utils.assert_interface(fun_decl, 'EagerFunctionDeclaration')

    contents = utils.get_field(fun_decl, 'contents')
    utils.assert_interface(contents, 'FunctionOrMethodContents')

    scope = utils.get_field(contents, 'parameterScope')
    utils.assert_interface(scope, 'AssertedParameterScope')

    param_names = utils.get_field(scope, 'paramNames')
    param_name = utils.get_element(param_names, 0)
    utils.assert_interface(param_name, 'AssertedPositionalParameterName')

    name = utils.get_field(param_name, 'name')

    utils.set_identifier_name(name, '1')

    return ast
