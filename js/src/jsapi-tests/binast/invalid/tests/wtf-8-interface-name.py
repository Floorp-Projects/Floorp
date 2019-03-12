def filter_ast(ast):
    # Put WTF-8 string into interface name.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('ExpressionStatement') \
        .set_interface_name(u'\uD83E_\uDD9D')

    return ast
