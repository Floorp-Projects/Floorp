import sys
import pymake.parser

for f in sys.argv[1:]:
    print "Parsing %s" % f
    fd = open(f)
    stmts = pymake.parser.parsestream(fd, f)
    print stmts
