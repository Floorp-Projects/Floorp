def filter_ast(ast):
    # Move function inside then-block, without fixing scope.
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    fun_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(fun_stmt, 'EagerFunctionDeclaration')

    if_stmt = utils.get_element(global_stmts, 1)
    utils.assert_interface(if_stmt, 'IfStatement')

    block_stmt = utils.get_field(if_stmt, 'consequent')
    utils.assert_interface(block_stmt, 'Block')

    block_stmts = utils.get_field(block_stmt, 'statements')

    utils.append_element(block_stmts, utils.copy_tagged_tuple(fun_stmt))

    return ast
