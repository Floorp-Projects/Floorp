# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from functools import partial

import mozunit
from moztest.selftest.output import get_mozharness_status, filter_action

from mozharness.base.log import INFO, WARNING
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WARNING

get_mozharness_status = partial(get_mozharness_status, 'reftest')


def test_output_pass(runtests):
    status, lines = runtests('reftest-pass.list')
    assert status == 0

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_SUCCESS
    assert log_level in (INFO, WARNING)

    test_end = filter_action('test_end', lines)
    assert len(test_end) == 3
    assert all(t['status'] == 'PASS' for t in test_end)


def test_output_fail(runtests):
    status, lines = runtests('reftest-fail.list')
    assert status == 0

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_WARNING
    assert log_level == WARNING

    test_end = filter_action('test_end', lines)
    assert len(test_end) == 3
    assert all(t['status'] == 'FAIL' for t in test_end)


if __name__ == '__main__':
    mozunit.main()
