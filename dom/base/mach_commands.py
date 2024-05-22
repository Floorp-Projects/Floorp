# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mach.decorators import Command


@Command(
    "gen-use-counter-metrics",
    category="misc",
    description="Generate a Glean use_counter_metrics.yaml file, creating metrics definitions for every use counter.",
)
def gen_use_counter_metrics(command_context):
    # Dispatch to usecounters.py
    import sys
    from os import path

    sys.path.append(path.dirname(__file__))
    from usecounters import gen_use_counter_metrics

    return gen_use_counter_metrics()


@Command(
    "gen-uuid",
    category="misc",
    description="Generate a uuid suitable for use in xpidl files and/or in C++",
)
def gen_uuid(command_context):
    import uuid

    uuid_str = str(uuid.uuid4())
    print(uuid_str)
    print()

    stuff = uuid_str.split("-")
    print(f"{{ 0x{stuff[0]}, 0x{stuff[1]}, 0x{stuff[2]}, \\")
    print(f"  {{ 0x{stuff[3][0:2]}, 0x{stuff[3][2:4]}, ", end="")
    print(f"0x{stuff[4][0:2]}, 0x{stuff[4][2:4]}, ", end="")
    print(f"0x{stuff[4][4:6]}, 0x{stuff[4][6:8]}, ", end="")
    print(f"0x{stuff[4][8:10]}, 0x{stuff[4][10:12]} }} }}")
