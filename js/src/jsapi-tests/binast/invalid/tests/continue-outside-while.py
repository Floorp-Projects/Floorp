def filter_ast(ast):
    # Move a continue statement out of a while loop.
    import filter_utils as utils

    global_stmts = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements')

    continue_stmt = global_stmts.elem(0) \
        .assert_interface('WhileStatement') \
        .field('body') \
        .assert_interface('Block') \
        .field('statements') \
        .remove_elem(0) \
        .assert_interface('ContinueStatement')

    global_stmts.append_elem(continue_stmt)

    return ast
