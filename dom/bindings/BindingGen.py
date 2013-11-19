# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import cPickle
from Configuration import Configuration
from Codegen import CGBindingRoot, replaceFileIfChanged, CGEventRoot
from mozbuild.makeutil import Makefile
from mozbuild.pythonutil import iter_modules_in_path
from buildconfig import topsrcdir


def generate_binding_files(config, outputprefix, srcprefix, webidlfile,
                           generatedEventsWebIDLFiles):
    """
    |config| Is the configuration object.
    |outputprefix| is a prefix to use for the header guards and filename.
    """

    depsname = ".deps/" + outputprefix + ".pp"
    root = CGBindingRoot(config, outputprefix, webidlfile)
    replaceFileIfChanged(outputprefix + ".h", root.declare())
    replaceFileIfChanged(outputprefix + ".cpp", root.define())

    if webidlfile in generatedEventsWebIDLFiles:
        eventName = webidlfile[:-len(".webidl")]
        generatedEvent = CGEventRoot(config, eventName)
        replaceFileIfChanged(eventName + ".h", generatedEvent.declare())
        replaceFileIfChanged(eventName + ".cpp", generatedEvent.define())

    mk = Makefile()
    # NOTE: it's VERY important that we output dependencies for the FooBinding
    # file here, not for the header or generated cpp file.  These dependencies
    # are used later to properly determine changedDeps and prevent rebuilding
    # too much.  See the comment explaining $(binding_dependency_trackers) in
    # Makefile.in.
    rule = mk.create_rule([outputprefix])
    rule.add_dependencies(os.path.join(srcprefix, x) for x in sorted(root.deps()))
    rule.add_dependencies(iter_modules_in_path(topsrcdir))
    with open(depsname, 'w') as f:
        mk.dump(f)

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

    # Load the configuration
    f = open('ParserResults.pkl', 'rb')
    config = cPickle.load(f)
    f.close()

    def readFile(f):
        file = open(f, 'rb')
        try:
            contents = file.read()
        finally:
            file.close()
        return contents
    allWebIDLFiles = readFile(args[2]).split()
    generatedEventsWebIDLFiles = readFile(args[3]).split()
    changedDeps = readFile(args[4]).split()

    if all(f.endswith("Binding") or f == "ParserResults.pkl" for f in changedDeps):
        toRegenerate = filter(lambda f: f.endswith("Binding"), changedDeps)
        if len(toRegenerate) == 0 and len(changedDeps) == 1:
            # Work around build system bug 874923: if we get here that means
            # that changedDeps contained only one entry and it was
            # "ParserResults.pkl".  That should never happen: if the
            # ParserResults.pkl changes then either one of the globalgen files
            # changed (in which case we wouldn't be in this "only
            # ParserResults.pkl and *Binding changed" code) or some .webidl
            # files changed (and then the corresponding *Binding files should
            # show up in changedDeps).  Since clearly the build system is
            # confused, just regenerate everything to be safe.
            toRegenerate = allWebIDLFiles
        else:
            toRegenerate = map(lambda f: f[:-len("Binding")] + ".webidl",
                               toRegenerate)
    else:
        toRegenerate = allWebIDLFiles

    for webIDLFile in toRegenerate:
        assert webIDLFile.endswith(".webidl")
        outputPrefix = webIDLFile[:-len(".webidl")] + "Binding"
        generate_binding_files(config, outputPrefix, srcPrefix, webIDLFile,
                               generatedEventsWebIDLFiles);

if __name__ == '__main__':
    main()
