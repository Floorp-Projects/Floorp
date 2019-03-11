def filter_ast(ast):
    # IdentifierExpression with non-identifier string.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    expr_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(expr_stmt, 'ExpressionStatement')

    assign_expr = utils.get_field(expr_stmt, 'expression')
    utils.assert_interface(assign_expr, 'AssignmentExpression')

    expr = utils.get_field(assign_expr, 'expression')
    utils.assert_interface(expr, 'IdentifierExpression')

    name = utils.get_field(expr, 'name')

    utils.set_identifier_name(name, '1')

    return ast
