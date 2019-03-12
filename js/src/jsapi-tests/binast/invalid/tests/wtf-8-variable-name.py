def filter_ast(ast):
    # Put WTF-8 string into identifier name.
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
        .set_identifier_name(u'\uD83E_\uDD9D')

    return ast
