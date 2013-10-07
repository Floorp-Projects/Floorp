#!/usr/bin/env python

"""
test dumpScreen functionality
"""

import automationutils
import optparse
import os
import sys


def main(args=sys.argv[1:]):

    # parse CLI options
    usage = '%prog [options] path/to/OBJDIR/dist/bin'
    parser = optparse.OptionParser(usage=usage)
    options, args = parser.parse_args(args)
    if len(args) != 1:
        parser.error("Please provide utility path")
    utilityPath = args[0]

    # dump the screen to a data: URL
    uri = automationutils.dumpScreen(utilityPath)

    # print the uri
    print uri

if __name__ == '__main__':
    main()
