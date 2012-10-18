# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import cPickle
import WebIDL
from Configuration import *
from Codegen import CGExampleRoot, replaceFileIfChanged
# import Codegen in general, so we can set a variable on it
import Codegen

def generate_interface_example(config, interfaceName):
    """
    |config| Is the configuration object.
    |interfaceName| is the name of the interface we're generating an example for.
    """

    root = CGExampleRoot(config, interfaceName)
    exampleHeader = interfaceName + "-example.h"
    exampleImpl = interfaceName + "-example.cpp"
    replaceFileIfChanged(exampleHeader, root.declare())
    replaceFileIfChanged(exampleImpl, root.define())

def main():

    # Parse arguments.
    from optparse import OptionParser
    usagestring = "usage: %prog configFile interfaceName"
    o = OptionParser(usage=usagestring)
    o.add_option("--verbose-errors", action='store_true', default=False,
                 help="When an error happens, display the Python traceback.")
    (options, args) = o.parse_args()

    if len(args) != 2:
        o.error(usagestring)
    configFile = os.path.normpath(args[0])
    interfaceName = args[1]

    # Load the parsing results
    f = open('ParserResults.pkl', 'rb')
    parserData = cPickle.load(f)
    f.close()

    # Create the configuration data.
    config = Configuration(configFile, parserData)

    # Generate the example class.
    generate_interface_example(config, interfaceName)

if __name__ == '__main__':
    main()
