def filter_ast(ast):
    # AssertedPositionalParameterName.index >= ARGNO_LIMIT - 1.
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
        .field('index') \
        .set_unsigned_long(65536)

    return ast
