def filter_ast(ast):
    # Set different parameter name than one in scope.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('EagerFunctionDeclaration') \
        .field('contents') \
        .assert_interface('FunctionOrMethodContents') \
        .field('params') \
        .assert_interface('FormalParameters') \
        .field('items') \
        .elem(0) \
        .assert_interface('BindingIdentifier') \
        .field('name') \
        .set_identifier_name('b')

    return ast
