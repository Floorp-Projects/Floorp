# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import pytest


@pytest.fixture
def parse(get_reftest, normalize):
    output = pytest.importorskip("output")

    reftest, options = get_reftest(tests=["dummy"])
    reftest._populate_logger(options)
    reftest.outputHandler = output.OutputHandler(
        reftest.log, options.utilityPath, options.symbolsPath
    )

    def resolve(path):
        path = normalize(path)
        return "file://{}".format(path)

    def inner(*manifests):
        assert len(manifests) > 0
        manifests = {m: (None, "id") for m in map(resolve, manifests)}
        return reftest.getActiveTests(manifests, options)

    return inner


def test_parse_test_types(parse):
    tests = parse("types.list")
    assert tests[0]["type"] == "=="
    assert tests[1]["type"] == "!="
    assert tests[2]["type"] == "load"
    assert tests[3]["type"] == "script"
    assert tests[4]["type"] == "print"


def test_parse_failure_type_interactions(parse):
    """Tests interactions between skip and fails."""
    tests = parse("failure-type-interactions.list")
    for t in tests:
        if "skip" in t["name"]:
            assert t["skip"]
        else:
            assert not t["skip"]

        # 0 => EXPECTED_PASS, 1 => EXPECTED_FAIL
        if "fails" in t["name"]:
            assert t["expected"] == 1
        else:
            assert t["expected"] == 0


def test_parse_invalid_manifests(parse):
    # XXX We should assert that the output contains the appropriate error
    # message, but we seem to be hitting an issue in pytest that is preventing
    # us from capturing the Gecko output with the capfd fixture. See:
    # https://github.com/pytest-dev/pytest/issues/5997
    with pytest.raises(SystemExit):
        parse("invalid-defaults.list")

    with pytest.raises(SystemExit):
        parse("invalid-defaults-include.list")

    with pytest.raises(SystemExit):
        parse("invalid-include.list")


if __name__ == "__main__":
    mozunit.main()
