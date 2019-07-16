#!/usr/bin/env python3

import json
import sys

j = json.load(sys.stdin)
components = sys.argv[1].split('/')

def next_match(json_fragment, components):
    if len(components) == 0:
        yield json_fragment
    else:
        component = components[0]
        if type(json_fragment) == list:
            if component == '*':
                for item in json_fragment:
                    yield from next_match(item, components[1:])
            else:
                component = int(component)
                if component >= len(j):
                    sys.exit(1)
                yield from next_match(json_fragment[component], components[1:])
        elif type(json_fragment) == dict:
            if component == '*':
                for key in sorted(json_fragment.keys()):
                    yield from next_match(json_fragment[key], components[1:])
            elif component not in json_fragment:
                sys.exit(1)
            else:
                yield from next_match(json_fragment[component], components[1:])

for match in list(next_match(j, components)):
    if type(match) == dict:
        print(' '.join(match.keys()))
    else:
        print(match)
