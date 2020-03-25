# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import codecs
import io
import itertools
import logging
import os
import sys
import textwrap


try:
    from collections.abc import Iterable
except ImportError:
    from collections import Iterable


base_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(base_dir, 'python', 'mozbuild'))
sys.path.insert(0, os.path.join(base_dir, 'third_party', 'python', 'six'))
from mozbuild.configure import (
    ConfigureSandbox,
    TRACE,
)
from mozbuild.pythonutil import iter_modules_in_path
from mozbuild.backend.configenvironment import PartialConfigEnvironment
from mozbuild.util import (
    write_indented_repr,
)
import mozpack.path as mozpath
import six


def main(argv):
    config = {}

    sandbox = ConfigureSandbox(config, os.environ, argv)

    clobber_file = 'CLOBBER'
    if not os.path.exists(clobber_file):
        # Simply touch the file.
        with open(clobber_file, 'a'):
            pass

    if os.environ.get('MOZ_CONFIGURE_TRACE'):
        sandbox._logger.setLevel(TRACE)

    sandbox.run(os.path.join(os.path.dirname(__file__), 'moz.configure'))

    if sandbox._help:
        return 0

    return config_status(config)


def check_unicode(obj):
    '''Recursively check that all strings in the object are unicode strings.'''
    if isinstance(obj, dict):
        result = True
        for k, v in six.iteritems(obj):
            if not check_unicode(k):
                print("%s key is not unicode." % k, file=sys.stderr)
                result = False
            elif not check_unicode(v):
                print("%s value is not unicode." % k, file=sys.stderr)
                result = False
        return result
    if isinstance(obj, bytes):
        return False
    if isinstance(obj, six.text_type):
        return True
    if isinstance(obj, Iterable):
        return all(check_unicode(o) for o in obj)
    return True


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
        k: sanitized_bools(v) for k, v in six.iteritems(config)
        if k not in ('DEFINES', 'non_global_defines', 'TOPSRCDIR', 'TOPOBJDIR',
                     'CONFIG_STATUS_DEPS')
    }
    sanitized_config['defines'] = {
        k: sanitized_bools(v) for k, v in six.iteritems(config['DEFINES'])
    }
    sanitized_config['non_global_defines'] = config['non_global_defines']
    sanitized_config['topsrcdir'] = config['TOPSRCDIR']
    sanitized_config['topobjdir'] = config['TOPOBJDIR']
    sanitized_config['mozconfig'] = config.get('MOZCONFIG')

    if not check_unicode(sanitized_config):
        print("Configuration should be all unicode.", file=sys.stderr)
        print("Please file a bug for the above.", file=sys.stderr)
        sys.exit(1)

    # Create config.status. Eventually, we'll want to just do the work it does
    # here, when we're able to skip configure tests/use cached results/not rely
    # on autoconf.
    logging.getLogger('moz.configure').info('Creating config.status')
    with codecs.open('config.status', 'w', 'utf-8') as fh:
        fh.write(textwrap.dedent('''\
            #!%(python)s
            # coding=utf-8
            from __future__ import unicode_literals
        ''') % {'python': config['PYTHON']})
        for k, v in six.iteritems(sanitized_config):
            fh.write('%s = ' % k)
            write_indented_repr(fh, v)
        fh.write("__all__ = ['topobjdir', 'topsrcdir', 'defines', "
                 "'non_global_defines', 'substs', 'mozconfig']")

        if config.get('MOZ_BUILD_APP') != 'js' or config.get('JS_STANDALONE'):
            fh.write(textwrap.dedent('''
                if __name__ == '__main__':
                    from mozbuild.util import patch_main
                    patch_main()
                    from mozbuild.config_status import config_status
                    args = dict([(name, globals()[name]) for name in __all__])
                    config_status(**args)
            '''))

    partial_config = PartialConfigEnvironment(config['TOPOBJDIR'])
    partial_config.write_vars(sanitized_config)

    # Write out a file so the build backend knows to re-run configure when
    # relevant Python changes.
    with io.open('config_status_deps.in', 'w', encoding='utf-8',
                 newline='\n') as fh:
        for f in itertools.chain(config['CONFIG_STATUS_DEPS'],
                                 iter_modules_in_path(config['TOPOBJDIR'],
                                                      config['TOPSRCDIR'])):
            fh.write('%s\n' % mozpath.normpath(f))

    # Other things than us are going to run this file, so we need to give it
    # executable permissions.
    os.chmod('config.status', 0o755)
    if config.get('MOZ_BUILD_APP') != 'js' or config.get('JS_STANDALONE'):
        from mozbuild.config_status import config_status

        # Some values in sanitized_config also have more complex types, such as
        # EnumString, which using when calling config_status would currently
        # break the build, as well as making it inconsistent with re-running
        # config.status, for which they are normalized to plain strings via
        # indented_repr. Likewise for non-dict non-string iterables being
        # converted to lists.
        def normalize(obj):
            if isinstance(obj, dict):
                return {
                    k: normalize(v)
                    for k, v in six.iteritems(obj)
                }
            if isinstance(obj, six.text_type):
                return six.text_type(obj)
            if isinstance(obj, Iterable):
                return [normalize(o) for o in obj]
            return obj
        return config_status(args=[], **normalize(sanitized_config))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
