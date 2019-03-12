def filter_ast(ast):
    # Put WTF-8 string into scope name.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('scope') \
        .assert_interface('AssertedScriptGlobalScope') \
        .field('declaredNames') \
        .elem(0) \
        .assert_interface('AssertedDeclaredName') \
        .field('name') \
        .set_identifier_name(u'\uD83E_\uDD9D')

    return ast
