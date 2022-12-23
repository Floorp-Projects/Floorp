# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import os
import re

import six

RE_COMMENT = re.compile(r"\s+#")
RE_HTTP = re.compile(r"HTTP\((\.\.(\/\.\.)*)\)")
RE_PROTOCOL = re.compile(r"^\w+:")
FAILURE_TYPES = (
    "fails",
    "fails-if",
    "needs-focus",
    "random",
    "random-if",
    "silentfail",
    "silentfail-if",
    "skip",
    "skip-if",
    "slow",
    "slow-if",
    "fuzzy",
    "fuzzy-if",
    "require-or",
    "asserts",
    "asserts-if",
)
PREF_ITEMS = (
    "pref",
    "test-pref",
    "ref-pref",
)
RE_ANNOTATION = re.compile(r"(.*)\((.*)\)")


class ReftestManifest(object):
    """Represents a parsed reftest manifest."""

    def __init__(self, finder=None):
        self.path = None
        self.dirs = set()
        self.files = set()
        self.manifests = set()
        self.tests = []
        self.finder = finder

    def load(self, path):
        """Parse a reftest manifest file."""

        def add_test(file, annotations, referenced_test=None):
            # We can't package about:, data:, or chrome: URIs.
            # Discarding data isn't correct for a parser. But retaining
            # all data isn't currently a requirement.
            if RE_PROTOCOL.match(file):
                return
            test = os.path.normpath(os.path.join(mdir, urlprefix + file))
            if test in self.files:
                # if test path has already been added, make no changes, to
                # avoid duplicate paths in self.tests
                return
            self.files.add(test)
            self.dirs.add(os.path.dirname(test))
            test_dict = {
                "path": test,
                "here": os.path.dirname(test),
                "manifest": normalized_path,
                "name": os.path.basename(test),
                "head": "",
                "support-files": "",
                "subsuite": "",
            }
            if referenced_test:
                test_dict["referenced-test"] = referenced_test
            for annotation in annotations:
                m = RE_ANNOTATION.match(annotation)
                if m:
                    if m.group(1) not in test_dict:
                        test_dict[m.group(1)] = m.group(2)
                    else:
                        test_dict[m.group(1)] += ";" + m.group(2)
                else:
                    test_dict[annotation] = None
            self.tests.append(test_dict)

        normalized_path = os.path.normpath(os.path.abspath(path))
        self.manifests.add(normalized_path)
        if not self.path:
            self.path = normalized_path

        mdir = os.path.dirname(normalized_path)
        self.dirs.add(mdir)

        if self.finder:
            lines = self.finder.get(path).read().splitlines()
        else:
            with io.open(path, "r", encoding="utf-8") as fh:
                lines = fh.read().splitlines()

        urlprefix = ""
        defaults = []
        for i, line in enumerate(lines):
            lineno = i + 1
            line = six.ensure_text(line)

            # Entire line is a comment.
            if line.startswith("#"):
                continue

            # Comments can begin mid line. Strip them.
            m = RE_COMMENT.search(line)
            if m:
                line = line[: m.start()]
            line = line.strip()
            if not line:
                continue

            items = line.split()
            if items[0] == "defaults":
                defaults = items[1:]
                continue

            items = defaults + items
            annotations = []
            for i in range(len(items)):
                item = items[i]

                if item.startswith(FAILURE_TYPES) or item.startswith(PREF_ITEMS):
                    annotations += [item]
                    continue
                if item == "HTTP":
                    continue

                m = RE_HTTP.match(item)
                if m:
                    # Need to package the referenced directory.
                    self.dirs.add(os.path.normpath(os.path.join(mdir, m.group(1))))
                    continue

                if i < len(defaults):
                    raise ValueError(
                        "Error parsing manifest {}, line {}: "
                        "Invalid defaults token '{}'".format(path, lineno, item)
                    )

                if item == "url-prefix":
                    urlprefix = items[i + 1]
                    break

                if item == "include":
                    self.load(os.path.join(mdir, items[i + 1]))
                    break

                if item == "load" or item == "script":
                    add_test(items[i + 1], annotations)
                    break

                if item == "==" or item == "!=" or item == "print":
                    add_test(items[i + 1], annotations)
                    add_test(items[i + 2], annotations, items[i + 1])
                    break
