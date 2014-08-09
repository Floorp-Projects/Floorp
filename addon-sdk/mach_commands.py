# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the xpcshell test runner with mach.

import os
import sys

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

class JetpackRunner(MozbuildObject):
    """Run jetpack tests."""
    def run_tests(self, **kwargs):
        self._run_make(target='jetpack-tests')

@CommandProvider
class MachCommands(MachCommandBase):
    @Command('jetpack-test', category='testing',
        description='Runs the jetpack test suite (Add-on SDK).')
    def run_jetpack_test(self, **params):
        # We should probably have a utility function to ensure the tree is
        # ready to run tests. Until then, we just create the state dir (in
        # case the tree wasn't built with mach).
        self._ensure_state_subdir_exists('.')

        jetpack = self._spawn(JetpackRunner)

        jetpack.run_tests(**params)
