def filter_ast(ast):
    # catch with missing AssertedBoundName
    import filter_utils as utils

    utils.assert_interface(ast, 'Script')
    global_stmts = utils.get_field(ast, 'statements')

    try_stmt = utils.get_element(global_stmts, 0)
    utils.assert_interface(try_stmt, 'TryCatchStatement')

    catch = utils.get_field(try_stmt, 'catchClause')
    utils.assert_interface(catch, 'CatchClause')

    scope = utils.get_field(catch, 'bindingScope')
    utils.assert_interface(scope, 'AssertedBoundNamesScope')

    names = utils.get_field(scope, 'boundNames')
    utils.remove_element(names, 0)

    return ast
