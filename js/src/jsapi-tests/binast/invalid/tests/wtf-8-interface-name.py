def filter_ast(ast):
    # Put WTF-8 string into interface name.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    expr_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(expr_stmt, 'ExpressionStatement')

    utils.set_interface_name(expr_stmt, u'\uD83E_\uDD9D')

    return ast
