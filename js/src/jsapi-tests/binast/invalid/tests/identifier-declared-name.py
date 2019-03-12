def filter_ast(ast):
    # AssertedDeclaredName with non-identifier string.
    import filter_utils as utils

    decl_names = utils.wrap(ast) \
        .assert_interface('Script') \
        .field('scope') \
        .assert_interface('AssertedScriptGlobalScope') \
        .field('declaredNames') \

    copied_decl_name = decl_names.elem(0) \
        .assert_interface('AssertedDeclaredName') \
        .copy()

    decl_names.append_elem(copied_decl_name)

    copied_decl_name.field('name') \
        .set_identifier_name('1')

    return ast
