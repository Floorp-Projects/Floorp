def filter_ast(ast):
    # IdentifierExpression with non-identifier string.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('ExpressionStatement') \
        .field('expression') \
        .assert_interface('AssignmentExpression') \
        .field('expression') \
        .assert_interface('IdentifierExpression') \
        .field('name') \
        .set_identifier_name('1')

    return ast
