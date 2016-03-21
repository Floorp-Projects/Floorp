# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import codecs
import json
import os
import subprocess
import sys


base_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(base_dir, 'python', 'mozbuild'))
from mozbuild.configure import ConfigureSandbox


def main(argv):
    config = {}
    sandbox = ConfigureSandbox(config, os.environ, argv)
    sandbox.run(os.path.join(os.path.dirname(__file__), 'moz.configure'))

    if sandbox._help:
        return 0

    return config_status(config)


def config_status(config):
    # Sanitize config data to feed config.status
    sanitized_config = {}
    sanitized_config['substs'] = {
        k: v for k, v in config.iteritems()
        if k not in ('DEFINES', 'non_global_defines', 'TOPSRCDIR', 'TOPOBJDIR')
    }
    sanitized_config['defines'] = config['DEFINES']
    sanitized_config['non_global_defines'] = config['non_global_defines']
    sanitized_config['topsrcdir'] = config['TOPSRCDIR']
    sanitized_config['topobjdir'] = config['TOPOBJDIR']

    # Create config.status. Eventually, we'll want to just do the work it does
    # here, when we're able to skip configure tests/use cached results/not rely
    # on autoconf.
    print("Creating config.status", file=sys.stderr)
    encoding = 'mbcs' if sys.platform == 'win32' else 'utf-8'
    with codecs.open('config.status', 'w', encoding) as fh:
        fh.write('#!%s\n' % config['PYTHON'])
        fh.write('# coding=%s\n' % encoding)
        # Because we're serializing as JSON but reading as python, the values
        # for True, False and None are true, false and null, which don't exist.
        # Define them.
        fh.write('true, false, null = True, False, None\n')
        for k, v in sanitized_config.iteritems():
            fh.write('%s = ' % k)
            json.dump(v, fh, sort_keys=True, indent=4, ensure_ascii=False)
            fh.write('\n')
        fh.write("__all__ = ['topobjdir', 'topsrcdir', 'defines', "
                 "'non_global_defines', 'substs']")

        if not config.get('BUILDING_JS') or config.get('JS_STANDALONE'):
            fh.write('''
if __name__ == '__main__':
    args = dict([(name, globals()[name]) for name in __all__])
    from mozbuild.config_status import config_status
    config_status(**args)
''')

    # Other things than us are going to run this file, so we need to give it
    # executable permissions.
    os.chmod('config.status', 0755)
    if not config.get('BUILDING_JS') or config.get('JS_STANDALONE'):
        if not config.get('JS_STANDALONE'):
            os.environ['WRITE_MOZINFO'] = '1'
        # Until we have access to the virtualenv from this script, execute
        # config.status externally, with the virtualenv python.
        return subprocess.call([config['PYTHON'], 'config.status'])
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
