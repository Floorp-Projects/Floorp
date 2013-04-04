# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import cPickle
from Configuration import Configuration
from Codegen import CGBindingRoot

def generate_binding_header(config, outputprefix, srcprefix, webidlfile):
    """
    |config| Is the configuration object.
    |outputprefix| is a prefix to use for the header guards and filename.
    """

    filename = outputprefix + ".h"
    depsname = ".deps/" + filename + ".pp"
    root = CGBindingRoot(config, outputprefix, webidlfile)
    with open(filename, 'wb') as f:
        f.write(root.declare())
    with open(depsname, 'wb') as f:
        # Sort so that our output is stable
        f.write("\n".join(filename + ": " + os.path.join(srcprefix, x) for
                          x in sorted(root.deps())))

def generate_binding_cpp(config, outputprefix, srcprefix, webidlfile):
    """
    |config| Is the configuration object.
    |outputprefix| is a prefix to use for the header guards and filename.
    """

    filename = outputprefix + ".cpp"
    depsname = ".deps/" + filename + ".pp"
    root = CGBindingRoot(config, outputprefix, webidlfile)
    with open(filename, 'wb') as f:
        f.write(root.define())
    with open(depsname, 'wb') as f:
        f.write("\n".join(filename + ": " + os.path.join(srcprefix, x) for
                          x in sorted(root.deps())))

def main():

    # Parse arguments.
    from optparse import OptionParser
    usagestring = "usage: %prog [header|cpp] configFile outputPrefix srcPrefix webIDLFile"
    o = OptionParser(usage=usagestring)
    o.add_option("--verbose-errors", action='store_true', default=False,
                 help="When an error happens, display the Python traceback.")
    (options, args) = o.parse_args()

    if len(args) != 5 or (args[0] != "header" and args[0] != "cpp"):
        o.error(usagestring)
    buildTarget = args[0]
    configFile = os.path.normpath(args[1])
    outputPrefix = args[2]
    srcPrefix = os.path.normpath(args[3])
    webIDLFile = os.path.normpath(args[4])

    # Load the parsing results
    f = open('ParserResults.pkl', 'rb')
    parserData = cPickle.load(f)
    f.close()

    # Create the configuration data.
    config = Configuration(configFile, parserData)

    # Generate the prototype classes.
    if buildTarget == "header":
        generate_binding_header(config, outputPrefix, srcPrefix, webIDLFile);
    elif buildTarget == "cpp":
        generate_binding_cpp(config, outputPrefix, srcPrefix, webIDLFile);
    else:
        assert False # not reached

if __name__ == '__main__':
    main()
