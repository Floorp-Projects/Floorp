# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import subprocess
import sys
from datetime import datetime

SOURCESTAMP_FILENAME = 'sourcestamp.txt'

def buildid_header(output):
    buildid = os.environ.get('MOZ_BUILD_DATE')
    if buildid and len(buildid) != 14:
        print('Ignoring invalid MOZ_BUILD_DATE: %s' % buildid, file=sys.stderr)
        buildid = None
    if not buildid:
        buildid = datetime.now().strftime('%Y%m%d%H%M%S')
    output.write("#define MOZ_BUILDID %s\n" % buildid)


def get_program_output(*command):
    try:
        with open(os.devnull) as stderr:
            return subprocess.check_output(command, stderr=stderr)
    except:
        return ''


def get_hg_info(workdir):
    repo = get_program_output('hg', '-R', workdir, 'path', 'default')
    if repo:
        repo = repo.strip()
        if repo.startswith('ssh://'):
            repo = 'https://' + repo[6:]
        repo = repo.rstrip('/')

    changeset = get_hg_changeset(workdir)

    return repo, changeset


def get_hg_changeset(path):
    return get_program_output('hg', '-R', path, 'parent', '--template={node}')

def get_info_from_sourcestamp(sourcestamp_path):
    """Read the repository and changelog information from the sourcestamp
    file. This assumes that the file exists and returns the results as a list
    (either strings or None in case of error).
    """

    # Load the content of the file.
    lines = None
    with open(sourcestamp_path) as f:
        lines = f.read().splitlines()

    # Parse the repo and the changeset. The sourcestamp file is supposed to
    # contain two lines: the first is the build id and the second is the source
    # URL.
    if len(lines) != 2 or not lines[1].startswith('http'):
        # Just return if the file doesn't contain what we expect.
        return None, None

    # Return the repo and the changeset.
    return lines[1].split('/rev/')

def source_repo_header(output):
    # We allow the source repo and changeset to be specified via the
    # environment (see configure)
    import buildconfig
    repo = buildconfig.substs.get('MOZ_SOURCE_REPO')
    changeset = buildconfig.substs.get('MOZ_SOURCE_CHANGESET')
    source = ''

    if not repo:
        sourcestamp_path = os.path.join(buildconfig.topsrcdir, SOURCESTAMP_FILENAME)
        if os.path.exists(os.path.join(buildconfig.topsrcdir, '.hg')):
            repo, changeset = get_hg_info(buildconfig.topsrcdir)
        elif os.path.exists(sourcestamp_path):
            repo, changeset = get_info_from_sourcestamp(sourcestamp_path)
    elif not changeset:
        changeset = get_hg_changeset(buildconfig.topsrcdir)
        if not changeset:
            raise Exception('could not resolve changeset; '
                            'try setting MOZ_SOURCE_CHANGESET')

    if changeset:
        output.write('#define MOZ_SOURCE_STAMP %s\n' % changeset)

    if repo and buildconfig.substs.get('MOZ_INCLUDE_SOURCE_INFO'):
        source = '%s/rev/%s' % (repo, changeset)
        output.write('#define MOZ_SOURCE_REPO %s\n' % repo)
        output.write('#define MOZ_SOURCE_URL %s\n' % source)


def main(args):
    if (len(args)):
        func = globals().get(args[0])
        if func:
            return func(sys.stdout, *args[1:])


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
