def filter_ast(ast):
    # AssignmentTargetIdentifier with non-identifier string.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('EagerFunctionDeclaration') \
        .field('name') \
        .assert_interface('BindingIdentifier') \
        .field('name') \
        .set_identifier_name('1')

    return ast
