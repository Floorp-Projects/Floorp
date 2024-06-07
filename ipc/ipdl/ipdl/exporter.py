# vim: set ts=4 sw=4 tw=99 et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import sys

from ipdl.ast import ASYNC, SYNC
from ipdl.ast import IN, OUT, INOUT
from ipdl.ast import StringLiteral

from pprint import pprint

# We export some protocol data to JSON so it can be consumed and processed
# further by external tooling, such as Fuzzing/Security to automatically
# determine security boundaries and generate code coverage filters for
# certain protocols.


class JSONExporter:
    @staticmethod
    def protocolToObject(protocol):
        def implToString(impl):
            if impl is None:
                return None

            if type(impl) is StringLiteral:
                return str(impl.value)

            return str(impl)

        p = {
            "name": protocol.name,
            "namespaces": [x.name for x in protocol.namespaces],
            "managers": [x.name for x in protocol.managers],
            "parent_methods": [],
            "child_methods": [],
            "attributes": {
                x.name: implToString(x.value) for x in protocol.attributes.values()
            },
        }

        for md in protocol.messageDecls:

            def serialize_md(md):
                return {
                    "name": md.decl.progname,
                    "attributes": {x.name: x.value for x in md.attributes.values()},
                    "sync": md.sendSemantics == SYNC,
                    "params": [
                        {"type": x.ipdltype.name(), "name": x.name} for x in md.params
                    ],
                }

            if md.direction == IN or md.direction == INOUT:
                p["parent_methods"].append(serialize_md(md))

            if md.direction == OUT or md.direction == INOUT:
                p["child_methods"].append(serialize_md(md))

        return p
