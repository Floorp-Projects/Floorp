# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import collections
import re


def read_conf(conf_filename):
    # Can't read/write from a single StringIO, so make a new one for reading.
    stream = open(conf_filename, "r")

    def parse_counters(stream):
        for line_num, line in enumerate(stream):
            line = line.rstrip("\n")
            if not line or line.startswith("//"):
                # empty line or comment
                continue
            m = re.match(r"method ([A-Za-z0-9]+)\.([A-Za-z0-9]+)$", line)
            if m:
                interface_name, method_name = m.groups()
                yield {
                    "type": "method",
                    "interface_name": interface_name,
                    "method_name": method_name,
                }
                continue
            m = re.match(r"attribute ([A-Za-z0-9]+)\.([A-Za-z0-9]+)$", line)
            if m:
                interface_name, attribute_name = m.groups()
                yield {
                    "type": "attribute",
                    "interface_name": interface_name,
                    "attribute_name": attribute_name,
                }
                continue
            m = re.match(r"custom ([A-Za-z0-9_]+) (.*)$", line)
            if m:
                name, desc = m.groups()
                yield {"type": "custom", "name": name, "desc": desc}
                continue
            raise ValueError(
                "error parsing %s at line %d" % (conf_filename, line_num + 1)
            )

    return parse_counters(stream)


def generate_histograms(filename, is_for_worker=False):
    # The mapping for use counters to telemetry histograms depends on the
    # ordering of items in the dictionary.

    # The ordering of the ending for workers depends on the WorkerType defined
    # in WorkerPrivate.h.
    endings = (
        ["DEDICATED_WORKER", "SHARED_WORKER", "SERVICE_WORKER"]
        if is_for_worker
        else ["DOCUMENT", "PAGE"]
    )

    items = collections.OrderedDict()
    for counter in read_conf(filename):

        def append_counter(name, desc):
            items[name] = {
                "expires_in_version": "never",
                "kind": "boolean",
                "description": desc,
            }

        def append_counters(name, desc):
            for ending in endings:
                append_counter(
                    "USE_COUNTER2_%s_%s" % (name, ending),
                    "Whether a %s %s" % (ending.replace("_", " ").lower(), desc),
                )

        if counter["type"] == "method":
            method = "%s.%s" % (counter["interface_name"], counter["method_name"])
            append_counters(method.replace(".", "_").upper(), "called %s" % method)
        elif counter["type"] == "attribute":
            attr = "%s.%s" % (counter["interface_name"], counter["attribute_name"])
            counter_name = attr.replace(".", "_").upper()
            append_counters("%s_getter" % counter_name, "got %s" % attr)
            append_counters("%s_setter" % counter_name, "set %s" % attr)
        elif counter["type"] == "custom":
            append_counters(counter["name"].upper(), counter["desc"])

    return items
