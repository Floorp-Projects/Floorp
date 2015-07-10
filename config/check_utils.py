# vim: set ts=8 sts=4 et sw=4 tw=99:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess

def get_all_toplevel_filenames():
    '''Get a list of all the files in the (Mercurial or Git) repository.'''
    try:
        cmd = ['hg', 'manifest', '-q']
        all_filenames = subprocess.check_output(cmd, universal_newlines=True,
                                                stderr=subprocess.PIPE).split('\n')
        return all_filenames
    except:
        pass

    try:
        # Get the relative path to the top-level directory.
        cmd = ['git', 'rev-parse', '--show-cdup']
        top_level = subprocess.check_output(cmd, universal_newlines=True,
                                                stderr=subprocess.PIPE).split('\n')[0]
        cmd = ['git', 'ls-files', '--full-name', top_level]
        all_filenames = subprocess.check_output(cmd, universal_newlines=True,
                                                stderr=subprocess.PIPE).split('\n')
        return all_filenames
    except:
        pass

    raise Exception('failed to run any of the repo manifest commands', cmds)
