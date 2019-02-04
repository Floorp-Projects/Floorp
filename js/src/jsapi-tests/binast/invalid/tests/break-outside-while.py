def filter_ast(ast):
    # Move a break statement out of a while loop.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    while_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(while_stmt, 'WhileStatement')

    while_body = utils.get_field(while_stmt, 'body')
    utils.assert_interface(while_body, 'Block')
    while_body_stmts = utils.get_field(while_body, 'statements')

    break_stmt = utils.get_element(while_body_stmts, 0)
    utils.assert_interface(break_stmt, 'BreakStatement')

    utils.remove_element(while_body_stmts, 0)
    utils.append_element(global_stmts, break_stmt)

    return ast
