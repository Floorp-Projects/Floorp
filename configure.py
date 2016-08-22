# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import codecs
import json
import os
import subprocess
import sys

from collections import Iterable


base_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(base_dir, 'python', 'mozbuild'))
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
    # Ideally, all the backend and frontend code would handle the booleans, but
    # there are so many things involved, that it's easier to keep config.status
    # untouched for now.
    def sanitized_bools(v):
        if v is True:
            return '1'
        if v is False:
            return ''
        return v

    sanitized_config = {}
    sanitized_config['substs'] = {
        k: sanitized_bools(v) for k, v in config.iteritems()
        if k not in ('DEFINES', 'non_global_defines', 'TOPSRCDIR', 'TOPOBJDIR')
    }
    sanitized_config['defines'] = {
        k: sanitized_bools(v) for k, v in config['DEFINES'].iteritems()
    }
    sanitized_config['non_global_defines'] = config['non_global_defines']
    sanitized_config['topsrcdir'] = config['TOPSRCDIR']
    sanitized_config['topobjdir'] = config['TOPOBJDIR']
    sanitized_config['mozconfig'] = config.get('MOZCONFIG')

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
                 "'non_global_defines', 'substs', 'mozconfig']")

        if config.get('MOZ_BUILD_APP') != 'js' or config.get('JS_STANDALONE'):
            fh.write('''
if __name__ == '__main__':
    args = dict([(name, globals()[name]) for name in __all__])
    from mozbuild.config_status import config_status
    config_status(**args)
''')

    # Running config.status standalone uses byte literals for all the config,
    # instead of the unicode literals we have in sanitized_config right now.
    # Some values in sanitized_config also have more complex types, such as
    # EnumString, which using when calling config_status would currently break
    # the build, as well as making it inconsistent with re-running
    # config.status. Fortunately, EnumString derives from unicode, so it's
    # covered by converting unicode strings.
    # Moreover, a lot of the build backend code is currently expecting byte
    # strings and breaks in subtle ways with unicode strings.
    def encode(v):
        if isinstance(v, dict):
            return {
                encode(k): encode(val)
                for k, val in v.iteritems()
            }
        if isinstance(v, str):
            return v
        if isinstance(v, unicode):
            return v.encode(encoding)
        if isinstance(v, Iterable):
            return [encode(i) for i in v]
        return v

    # Other things than us are going to run this file, so we need to give it
    # executable permissions.
    os.chmod('config.status', 0o755)
    if config.get('MOZ_BUILD_APP') != 'js' or config.get('JS_STANDALONE'):
        os.environ[b'WRITE_MOZINFO'] = b'1'
        from mozbuild.config_status import config_status
        return config_status(args=[], **encode(sanitized_config))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
