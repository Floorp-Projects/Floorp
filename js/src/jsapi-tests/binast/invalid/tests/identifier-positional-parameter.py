def filter_ast(ast):
    # AssertedPositionalParameterName with non-identifier string.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('EagerFunctionDeclaration') \
        .field('contents') \
        .assert_interface('FunctionOrMethodContents') \
        .field('parameterScope') \
        .assert_interface('AssertedParameterScope') \
        .field('paramNames') \
        .elem(0) \
        .assert_interface('AssertedPositionalParameterName') \
        .field('name') \
        .set_identifier_name('1')

    return ast
