# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import mozunit
import pytest


@pytest.fixture
def parse(get_reftest, normalize):
    output = pytest.importorskip("output")

    reftest, options = get_reftest(tests=["dummy"])
    reftest._populate_logger(options)
    reftest.outputHandler = output.OutputHandler(
        reftest.log, options.utilityPath, options.symbolsPath)

    def resolve(path):
        path = normalize(path)
        return "file://{}".format(path)

    def inner(*manifests):
        assert len(manifests) > 0
        manifests = {m: None for m in map(resolve, manifests)}
        return reftest.getActiveTests(manifests, options)

    return inner


def test_parse_test_types(parse):
    tests = parse('types.list')
    assert tests[0]['type'] == '=='
    assert tests[1]['type'] == '!='
    assert tests[2]['type'] == 'load'
    assert tests[3]['type'] == 'script'
    assert tests[4]['type'] == 'print'


if __name__ == '__main__':
    mozunit.main()
