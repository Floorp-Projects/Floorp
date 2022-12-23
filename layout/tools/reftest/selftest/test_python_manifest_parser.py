# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import pytest


@pytest.fixture
def parse(normalize):
    reftest = pytest.importorskip("reftest")

    def inner(path):
        mp = reftest.ReftestManifest()
        mp.load(normalize(path))
        return mp

    return inner


def test_parse_defaults(parse):
    mp = parse("defaults.list")
    assert len(mp.tests) == 4

    for test in mp.tests:
        if test["name"].startswith("foo"):
            assert test["pref"] == "foo.bar,true"
        else:
            assert "pref" not in test

    # invalid defaults
    with pytest.raises(ValueError):
        parse("invalid-defaults.list")


if __name__ == "__main__":
    mozunit.main()
