# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# We do one global pass over all the WebIDL to generate our prototype enum
# and generate information for subsequent phases.

import os
import WebIDL
import cPickle
from Configuration import Configuration
from Codegen import GlobalGenRoots, replaceFileIfChanged

def generate_file(config, name, action):

    root = getattr(GlobalGenRoots, name)(config)
    if action is 'declare':
        filename = name + '.h'
        code = root.declare()
    else:
        assert action is 'define'
        filename = name + '.cpp'
        code = root.define()

    if replaceFileIfChanged(filename, code):
        print "Generating %s" % (filename)
    else:
        print "%s hasn't changed - not touching it" % (filename)

def main():
    # Parse arguments.
    from optparse import OptionParser
    usageString = "usage: %prog [options] webidldir [files]"
    o = OptionParser(usage=usageString)
    o.add_option("--cachedir", dest='cachedir', default=None,
                 help="Directory in which to cache lex/parse tables.")
    o.add_option("--verbose-errors", action='store_true', default=False,
                 help="When an error happens, display the Python traceback.")
    (options, args) = o.parse_args()

    if len(args) < 2:
        o.error(usageString)

    configFile = args[0]
    baseDir = args[1]
    fileList = args[2:]

    # Parse the WebIDL.
    parser = WebIDL.Parser(options.cachedir)
    for filename in fileList:
        fullPath = os.path.normpath(os.path.join(baseDir, filename))
        f = open(fullPath, 'rb')
        lines = f.readlines()
        f.close()
        parser.parse(''.join(lines), fullPath)
    parserResults = parser.finish()

    # Load the configuration.
    config = Configuration(configFile, parserResults)

    # Write the configuration out to a pickle.
    resultsFile = open('ParserResults.pkl', 'wb')
    cPickle.dump(config, resultsFile, -1)
    resultsFile.close()

    # Generate the atom list.
    generate_file(config, 'GeneratedAtomList', 'declare')

    # Generate the prototype list.
    generate_file(config, 'PrototypeList', 'declare')

    # Generate the common code.
    generate_file(config, 'RegisterBindings', 'declare')
    generate_file(config, 'RegisterBindings', 'define')

    generate_file(config, 'UnionTypes', 'declare')
    generate_file(config, 'UnionTypes', 'define')
    generate_file(config, 'UnionConversions', 'declare')

if __name__ == '__main__':
    main()
