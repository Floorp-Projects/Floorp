def filter_ast(ast):
    # Put WTF-8 string into identifier name.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    expr_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(expr_stmt, 'ExpressionStatement')

    assign_expr = utils.get_field(expr_stmt, 'expression')
    utils.assert_interface(assign_expr, 'AssignmentExpression')

    binding = utils.get_field(assign_expr, 'binding')
    utils.assert_interface(binding, 'AssignmentTargetIdentifier')

    name = utils.get_field(binding, 'name')
    utils.set_identifier_name(name, u'\uD83E_\uDD9D')

    return ast
