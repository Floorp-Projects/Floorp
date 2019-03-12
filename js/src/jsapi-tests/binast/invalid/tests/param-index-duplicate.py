def filter_ast(ast):
    # 2 AssertedPositionalParameterName with same index.
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
        .elem(1) \
        .assert_interface('AssertedPositionalParameterName') \
        .field('index') \
        .set_unsigned_long(0)

    return ast
