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
