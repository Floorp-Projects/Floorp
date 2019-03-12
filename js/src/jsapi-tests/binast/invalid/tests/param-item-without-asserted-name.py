def filter_ast(ast):
    # Add unmatching PositionalFormalParameter
    import filter_utils as utils

    params_items = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('EagerFunctionDeclaration') \
        .field('contents') \
        .assert_interface('FunctionOrMethodContents') \
        .field('params') \
        .assert_interface('FormalParameters') \
        .field('items')

    copied_param = params_items.elem(0) \
        .assert_interface('BindingIdentifier') \
        .copy()

    copied_param.field('name') \
        .set_identifier_name('b')

    params_items.append_elem(copied_param)

    return ast
