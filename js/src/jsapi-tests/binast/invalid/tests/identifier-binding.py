def filter_ast(ast):
    # BindingIdentifier with non-identifier string.
    import filter_utils as utils

    decls = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('VariableDeclaration') \
        .field('declarators')

    copied_decl = decls.elem(0) \
        .assert_interface('VariableDeclarator') \
        .copy()

    decls.append_elem(copied_decl)

    copied_decl.field('binding') \
        .assert_interface('BindingIdentifier') \
        .field('name') \
        .set_identifier_name('1')

    return ast
