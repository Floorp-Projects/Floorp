def filter_ast(ast):
    # AssertedBoundName with non-identifier string.
    import filter_utils as utils

    utils.wrap(ast) \
        .assert_interface('Script') \
        .field('statements') \
        .elem(0) \
        .assert_interface('TryCatchStatement') \
        .field('catchClause') \
        .assert_interface('CatchClause') \
        .field('bindingScope') \
        .assert_interface('AssertedBoundNamesScope') \
        .field('boundNames') \
        .elem(0) \
        .assert_interface('AssertedBoundName') \
        .field('name') \
        .set_identifier_name('1')

    return ast
