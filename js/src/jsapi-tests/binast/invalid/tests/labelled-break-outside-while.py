def filter_ast(ast):
    # Move a break statement out of a while loop.
    import filter_utils as utils

    global_stmts = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements')

    break_stmt = global_stmts.elem(0) \
        .assert_interface('LabelledStatement') \
        .field('body') \
        .assert_interface('WhileStatement') \
        .field('body') \
        .assert_interface('Block') \
        .field('statements') \
        .remove_elem(0) \
        .assert_interface('BreakStatement')

    global_stmts.append_elem(break_stmt)

    return ast
