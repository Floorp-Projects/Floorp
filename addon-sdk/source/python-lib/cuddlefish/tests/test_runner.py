# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def xulrunner_app_runner_doctests():
    """
    >>> import sys
    >>> from cuddlefish import runner
    >>> runner.XulrunnerAppRunner(binary='foo')
    Traceback (most recent call last):
    ...
    Exception: Binary path does not exist foo

    >>> runner.XulrunnerAppRunner(binary=sys.executable)
    Traceback (most recent call last):
    ...
    ValueError: application.ini not found in cmdargs

    >>> runner.XulrunnerAppRunner(binary=sys.executable,
    ...                         cmdargs=['application.ini'])
    Traceback (most recent call last):
    ...
    ValueError: file does not exist: 'application.ini'
    """

    pass
