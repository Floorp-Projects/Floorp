# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import cPickle
from Configuration import Configuration
from Codegen import CGBindingRoot, replaceFileIfChanged

def generate_binding_files(config, outputprefix, srcprefix, webidlfile):
    """
    |config| Is the configuration object.
    |outputprefix| is a prefix to use for the header guards and filename.
    """

    depsname = ".deps/" + outputprefix + ".pp"
    root = CGBindingRoot(config, outputprefix, webidlfile)
    replaceFileIfChanged(outputprefix + ".h", root.declare())
    replaceFileIfChanged(outputprefix + ".cpp", root.define())

    with open(depsname, 'wb') as f:
        # Sort so that our output is stable
        f.write("\n".join(outputprefix + ": " + os.path.join(srcprefix, x) for
                          x in sorted(root.deps())))

def main():
    # Parse arguments.
    from optparse import OptionParser
    usagestring = "usage: %prog [header|cpp] configFile outputPrefix srcPrefix webIDLFile"
    o = OptionParser(usage=usagestring)
    o.add_option("--verbose-errors", action='store_true', default=False,
                 help="When an error happens, display the Python traceback.")
    (options, args) = o.parse_args()

    configFile = os.path.normpath(args[0])
    srcPrefix = os.path.normpath(args[1])

    # Load the parsing results
    f = open('ParserResults.pkl', 'rb')
    parserData = cPickle.load(f)
    f.close()

    # Create the configuration data.
    config = Configuration(configFile, parserData)

    def readFile(f):
        file = open(f, 'rb')
        try:
            contents = file.read()
        finally:
            file.close()
        return contents
    allWebIDLFiles = readFile(args[2]).split()
    changedDeps = readFile(args[3]).split()

    if all(f.endswith("Binding") or f == "ParserResults.pkl" for f in changedDeps):
        toRegenerate = filter(lambda f: f.endswith("Binding"), changedDeps)
        toRegenerate = map(lambda f: f[:-len("Binding")] + ".webidl", toRegenerate)
    else:
        toRegenerate = allWebIDLFiles

    for webIDLFile in toRegenerate:
        assert webIDLFile.endswith(".webidl")
        outputPrefix = webIDLFile[:-len(".webidl")] + "Binding"
        generate_binding_files(config, outputPrefix, srcPrefix, webIDLFile);

if __name__ == '__main__':
    main()
