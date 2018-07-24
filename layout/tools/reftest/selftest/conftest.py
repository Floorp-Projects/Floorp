# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
from argparse import Namespace
from cStringIO import StringIO

import mozinfo
import pytest
from manifestparser import expression
from moztest.selftest.fixtures import binary, setup_test_harness  # noqa

here = os.path.abspath(os.path.dirname(__file__))
setup_args = [False, 'reftest', 'reftest']


@pytest.fixture  # noqa: F811
def parser(setup_test_harness):
    setup_test_harness(*setup_args)
    cmdline = pytest.importorskip('reftestcommandline')
    return cmdline.DesktopArgumentsParser()


@pytest.fixture  # noqa: F811
def runtests(setup_test_harness, binary, parser):
    setup_test_harness(*setup_args)
    runreftest = pytest.importorskip('runreftest')
    harness_root = runreftest.SCRIPT_DIRECTORY

    buf = StringIO()
    build = parser.build_obj
    options = vars(parser.parse_args([]))
    options.update({
        'app': binary,
        'focusFilterMode': 'non-needs-focus',
        'log_raw': [buf],
        'suite': 'reftest',
        'specialPowersExtensionPath': os.path.join(harness_root, 'specialpowers'),
    })

    if not os.path.isdir(build.bindir):
        package_root = os.path.dirname(harness_root)
        options.update({
            'extraProfileFiles': [os.path.join(package_root, 'bin', 'plugins')],
            'reftestExtensionPath': os.path.join(harness_root, 'reftest'),
            'sandboxReadWhitelist': [here, os.environ['PYTHON_TEST_TMP']],
            'utilityPath': os.path.join(package_root, 'bin'),
        })

        if 'MOZ_FETCHES_DIR' in os.environ:
            options['sandboxReadWhitelist'].append(os.environ['MOZ_FETCHES_DIR'])
    else:
        options.update({
            'extraProfileFiles': [os.path.join(build.topobjdir, 'dist', 'plugins')],
            'sandboxReadWhitelist': [build.topobjdir, build.topsrcdir],
        })

    def normalize(test):
        if os.path.isabs(test):
            return test
        return os.path.join(here, 'files', test)

    def inner(*tests, **opts):
        assert len(tests) > 0
        tests = map(normalize, tests)

        options['tests'] = tests
        options.update(opts)

        result = runreftest.run_test_harness(parser, Namespace(**options))
        out = json.loads('[' + ','.join(buf.getvalue().splitlines()) + ']')
        buf.close()
        return result, out
    return inner


@pytest.fixture(autouse=True)  # noqa: F811
def skip_using_mozinfo(request, setup_test_harness):
    """Gives tests the ability to skip based on values from mozinfo.

    Example:
        @pytest.mark.skip_mozinfo("!e10s || os == 'linux'")
        def test_foo():
            pass
    """

    setup_test_harness(*setup_args)
    runreftest = pytest.importorskip('runreftest')
    runreftest.update_mozinfo()

    skip_mozinfo = request.node.get_marker('skip_mozinfo')
    if skip_mozinfo:
        value = skip_mozinfo.args[0]
        if expression.parse(value, **mozinfo.info):
            pytest.skip("skipped due to mozinfo match: \n{}".format(value))
