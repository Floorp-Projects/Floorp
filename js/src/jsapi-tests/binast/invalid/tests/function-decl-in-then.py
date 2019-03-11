def filter_ast(ast):
    # Move function inside then-block, without fixing scope.
    import filter_utils as utils

    global_stmts = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements')

    fun_decl = global_stmts \
        .elem(0) \
        .assert_interface('EagerFunctionDeclaration')

    global_stmts \
        .elem(1) \
        .assert_interface('IfStatement') \
        .field('consequent') \
        .assert_interface('Block') \
        .field('statements') \
        .append_elem(fun_decl.copy())

    return ast
