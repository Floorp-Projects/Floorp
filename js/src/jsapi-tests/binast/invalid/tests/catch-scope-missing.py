def filter_ast(ast):
    # catch with missing AssertedBoundName
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
        .remove_elem(0)

    return ast
