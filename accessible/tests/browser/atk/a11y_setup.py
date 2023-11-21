# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Python environment for ATK a11y browser tests.
"""

import os
import subprocess
import sys

import psutil

# pyatspi can't be installed using pip. Rely on the system installation.
# Get the path to the system installation of pyatspi.
pyatspiFile = subprocess.check_output(
    (
        os.path.join(sys.base_prefix, "bin", "python3"),
        "-c",
        "import pyatspi; print(pyatspi.__file__)",
    ),
    encoding="utf-8",
).rstrip()
sys.path.append(os.path.dirname(os.path.dirname(pyatspiFile)))
import pyatspi

sys.path.pop()
del pyatspiFile


def getDoc():
    """Get the Accessible for the document being tested."""
    # We can compare the parent process ids to find the Firefox started by the
    # test harness.
    commonPid = psutil.Process().ppid()
    for app in pyatspi.Registry.getDesktop(0):
        if (
            app.name == "Firefox"
            and psutil.Process(app.get_process_id()).ppid() == commonPid
        ):
            break
    else:
        raise LookupError("Couldn't find Firefox application Accessible")
    root = app[0]
    for embeds in root.getRelationSet():
        if embeds.getRelationType() == pyatspi.RELATION_EMBEDS:
            break
    else:
        raise LookupError("Firefox root doesn't have RELATION_EMBEDS")
    doc = embeds.getTarget(0)
    child = doc[0]
    if child.get_attributes().get("id") == "default-iframe-id":
        # This is an iframe or remoteIframe test.
        doc = child[0]
    return doc


def findByDomId(root, id):
    for child in root:
        if child.get_attributes().get("id") == id:
            return child
        descendant = findByDomId(child, id)
        if descendant:
            return descendant
