def filter_ast(ast):
    # Put WTF-8 string into field name.
    # In multipart format, field name is not encoded into the file,
    # so this has no effect.
    import filter_utils as utils

    expr_stmt = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('ExpressionStatement') \

    expr_stmt.append_field(u'\uD83E_\uDD9D',
                           expr_stmt.remove_field('expression'))

    return ast
