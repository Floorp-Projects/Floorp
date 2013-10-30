#!/usr/bin/python

import re
import argparse
import sys

parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument('rootingHazards', nargs='?', default='rootingHazards.txt')
parser.add_argument('gcFunctions', nargs='?', default='gcFunctions.txt')
parser.add_argument('hazards', nargs='?', default='hazards.txt')
parser.add_argument('extra', nargs='?', default='unnecessary.txt')
parser.add_argument('refs', nargs='?', default='refs.txt')
parser.add_argument('--expect-hazards', type=int, default=None)
parser.add_argument('--expect-refs', type=int, default=None)
parser.add_argument('--expect-file', type=str, default=None)
args = parser.parse_args()

if args.expect_file:
    import json
    data = json.load(file(args.expect_file, 'r'))
    args.expect_hazards = args.expect_hazards or data.get('expect-hazards')
    args.expect_refs = args.expect_refs or data.get('expect-refs')

num_hazards = 0
num_refs = 0
try:
    with open(args.rootingHazards) as rootingHazards, \
        open(args.hazards, 'w') as hazards, \
        open(args.extra, 'w') as extra, \
        open(args.refs, 'w') as refs:
        current_gcFunction = None

        # Map from a GC function name to the list of hazards resulting from
        # that GC function
        hazardousGCFunctions = {}

        # List of tuples (gcFunction, index of hazard) used to maintain the
        # ordering of the hazards
        hazardOrder = []

        for line in rootingHazards:
            m = re.match(r'^Time: (.*)', line)
            mm = re.match(r'^Run on:', line)
            if m or mm:
                print >>hazards, line
                print >>extra, line
                print >>refs, line
                continue

            m = re.match(r'^Function.*has unnecessary root', line)
            if m:
                print >>extra, line
                continue

            m = re.match(r'^Function.*takes unsafe address of unrooted', line)
            if m:
                num_refs += 1
                print >>refs, line
                continue

            m = re.match(r"^Function.*has unrooted.*of type.*live across GC call '(.*?)'", line)
            if m:
                current_gcFunction = m.group(1)
                hazardousGCFunctions.setdefault(current_gcFunction, []).append(line);
                hazardOrder.append((current_gcFunction, len(hazardousGCFunctions[current_gcFunction]) - 1))
                num_hazards += 1
                continue

            if current_gcFunction:
                if not line.strip():
                    # Blank line => end of this hazard
                    current_gcFunction = None
                else:
                    hazardousGCFunctions[current_gcFunction][-1] += line

        with open(args.gcFunctions) as gcFunctions:
            gcExplanations = {} # gcFunction => stack showing why it can GC

            current_func = None
            for line in gcFunctions:
                m = re.match(r'^GC Function: (.*)', line)
                if m:
                    if current_func:
                        gcExplanations[current_func] = explanation
                    current_func = None
                    if m.group(1) in hazardousGCFunctions:
                        current_func = m.group(1)
                        explanation = line
                elif current_func:
                    explanation += line
            if current_func:
                gcExplanations[current_func] = explanation

            for gcFunction, index in hazardOrder:
                gcHazards = hazardousGCFunctions[gcFunction]
                print >>hazards, (gcHazards[index] + gcExplanations[gcFunction])

except IOError as e:
    print 'Failed: %s' % str(e)

print("Wrote %s" % args.hazards)
print("Wrote %s" % args.extra)
print("Wrote %s" % args.refs)
print("Found %d hazards and %d unsafe references" % (num_hazards, num_refs))

if args.expect_hazards is not None and args.expect_hazards != num_hazards:
    if args.expect_hazards < num_hazards:
        print("TEST-UNEXPECTED-FAILURE: %d more hazards than expected (expected %d, saw %d)" % (num_hazards - args.expect_hazards, args.expect_hazards, num_hazards))
        sys.exit(1)
    else:
        print("%d fewer hazards than expected! (expected %d, saw %d)" % (args.expect_hazards - num_hazards, args.expect_hazards, num_hazards))

if args.expect_refs is not None and args.expect_refs != num_refs:
    if args.expect_refs < num_refs:
        print("TEST-UNEXPECTED-FAILURE: %d more unsafe refs than expected (expected %d, saw %d)" % (num_refs - args.expect_refs, args.expect_refs, num_refs))
        sys.exit(1)
    else:
        print("%d fewer unsafe refs than expected! (expected %d, saw %d)" % (args.expect_refs - num_refs, args.expect_refs, num_refs))

sys.exit(0)
