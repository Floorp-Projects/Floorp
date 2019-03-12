def filter_ast(ast):
    # Duplicate parameter up to 65536 entries.
    import filter_utils as utils

    param_names = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('EagerFunctionDeclaration') \
        .field('contents') \
        .assert_interface('FunctionOrMethodContents') \
        .field('parameterScope') \
        .assert_interface('AssertedParameterScope') \
        .field('paramNames')

    param_name = param_names.elem(0) \
        .assert_interface('AssertedPositionalParameterName')

    for i in range(1, 65536 + 1):
        copied_param_name = param_name.copy()

        copied_param_name.field('index') \
            .set_unsigned_long(i)

        copied_param_name.field('name') \
            .set_identifier_name('a{}'.format(i))

        param_names.append_elem(copied_param_name)

    return ast
