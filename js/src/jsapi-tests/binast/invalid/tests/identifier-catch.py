def filter_ast(ast):
    # AssertedBoundName with non-identifier string.
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
    bound_name = utils.get_element(names, 0)
    utils.assert_interface(bound_name, 'AssertedBoundName')

    name = utils.get_field(bound_name, 'name')

    utils.set_identifier_name(name, '1')

    return ast
