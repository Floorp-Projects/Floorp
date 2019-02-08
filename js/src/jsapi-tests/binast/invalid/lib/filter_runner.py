# A utility to run filter program for given JSON format BinAST.

import json
import runpy


def run(filter_path, source):
    """
    Run filter script against JSON source.

    :param filter_path(string)
           The path to the filter script (python script)
    :param source(string)
           The string representation of JSON format of BinAST file
    :returns (string)
             The string representation of filtered JSON
    """
    ast = json.loads(source)

    # The filter script is executed with sys.path that has this directory in
    # the first element, so that it can import filter_utils.
    filter_global = runpy.run_path(filter_path)

    filtered_ast = filter_global['filter_ast'](ast)

    return json.dumps(filtered_ast)
