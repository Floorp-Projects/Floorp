def filter_ast(ast):
    # Duplicate parameter up to 65536 entries.
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

    for i in range(1, 65536 + 1):
        copied_param_name = utils.copy_tagged_tuple(param_name)
        index = utils.get_field(copied_param_name, "index")
        name = utils.get_field(copied_param_name, "name")

        utils.set_unsigned_long(index, i)
        utils.set_identifier_name(name, "a{}".format(i))

        utils.append_element(param_names, copied_param_name)

    return ast
