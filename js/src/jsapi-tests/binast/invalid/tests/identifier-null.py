def filter_ast(ast):
    # AssignmentTargetIdentifier with null string.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('ExpressionStatement') \
        .field('expression') \
        .assert_interface('AssignmentExpression') \
        .field('binding') \
        .assert_interface('AssignmentTargetIdentifier') \
        .field('name') \
        .set_null_identifier_name()

    return ast
