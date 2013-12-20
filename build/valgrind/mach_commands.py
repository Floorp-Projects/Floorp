# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import subprocess

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandProvider,
    Command,
)


@CommandProvider
class MachCommands(MachCommandBase):
    '''
    Easily run Valgrind tests.
    '''
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

    @Command('valgrind-test', category='testing',
        conditions=[conditions.is_firefox],
        description='Run the Valgrind test job.')
    def valgrind_test(self):
        defines = self.config_environment.defines
        if 'MOZ_VALGRIND' not in defines or 'MOZ_MEMORY' in defines:
            print("sorry, this command requires a build configured with\n"
                  "--enable-valgrind and --disable-jemalloc build")
            return 1

        debugger_args = [
            '--error-exitcode=1',
            '--smc-check=all-non-file',
            '--vex-iropt-register-updates=allregs-at-each-insn',
            '--gen-suppressions=all',
            '--num-callers=20',
            '--leak-check=full',
            '--show-possibly-lost=no',
            '--track-origins=yes'
        ]

        build_dir = os.path.join(self.topsrcdir, 'build')
        supps_dir = os.path.join(build_dir, 'valgrind')
        debugger_args.append('--suppressions=' + os.path.join(supps_dir, 'cross-architecture.sup'))

        # MACHTYPE is an odd bash-only environment variable that doesn't show
        # up in os.environ, so we have to get it another way.
        machtype = subprocess.check_output(['bash', '-c', 'echo $MACHTYPE']).rstrip()
        arch_specific_supps_file = os.path.join(supps_dir, machtype + '.sup')
        if os.path.isfile(arch_specific_supps_file):
            debugger_args.append('--suppressions=' + os.path.join(supps_dir, arch_specific_supps_file))
            print('Using platform-specific suppression file: ',
                  arch_specific_supps_file + '\n')
        else:
            print('Warning: could not find a platform-specific suppression file\n')

        env = os.environ.copy()
        env['G_SLICE'] = 'always-malloc'
        env['XPCOM_CC_RUN_DURING_SHUTDOWN'] = '1'

        script = os.path.join(build_dir, 'valgrind', 'valgrind_test.py')


        return subprocess.call([self.virtualenv_manager.python_path, script,
                                '--debugger=valgrind',
                                '--debugger-args=' + ' '.join(debugger_args) + ''],
                                env=env)

