# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the xpcshell test runner with mach.

from __future__ import absolute_import

import os

import mozpack.path as mozpath

from mozbuild.base import (
    MachCommandBase,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

@CommandProvider
class MachCommands(MachCommandBase):
    @Command('generate-addon-sdk-moz-build', category='misc',
        description='Generates the moz.build file for the addon-sdk/ directory.')
    def run_addon_sdk_moz_build(self, **params):
        addon_sdk_dir = mozpath.join(self.topsrcdir, 'addon-sdk')
        js_src_dir = mozpath.join(addon_sdk_dir, 'source/lib')
        dirs_to_files = {}

        for path, dirs, files in os.walk(js_src_dir):
            js_files = [f for f in files if f.endswith(('.js', '.jsm', '.html'))]
            if not js_files:
                continue

            relative = mozpath.relpath(path, js_src_dir)
            dirs_to_files[relative] = js_files

        moz_build = """# AUTOMATICALLY GENERATED FROM mozbuild.template AND mach.  DO NOT EDIT.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

%(moz-build-template)s
if CONFIG['MOZ_WIDGET_TOOLKIT'] != "gonk":
%(non-b2g-modules)s
%(always-on-modules)s"""

        non_b2g_paths = [
            'method/test',
            'sdk/ui',
            'sdk/ui/button',
            'sdk/ui/sidebar',
            'sdk/places',
            'sdk/places/host',
            'sdk/tabs',
            'sdk/panel',
            'sdk/frame',
            'sdk/test',
            'sdk/window',
            'sdk/windows',
            'sdk/deprecated',
        ]

        non_b2g_modules = []
        always_on_modules = []

        for d, files in sorted(dirs_to_files.items()):
            if d in non_b2g_paths:
                non_b2g_modules.append((d, files))
            else:
                always_on_modules.append((d, files))

        def list_to_js_modules(l, indent=''):
            js_modules = []
            for d, files in l:
                if d == '':
                    module_path = ''
                    dir_path = ''
                else:
                    # Ensure that we don't have things like:
                    #   EXTRA_JS_MODULES.commonjs.sdk.private-browsing
                    # which would be a Python syntax error.
                    path = d.split('/')
                    module_path = ''.join('.' + p if p.find('-') == -1 else "['%s']" % p for p in path)
                    dir_path = d + '/'
                filelist = ["'source/lib/%s%s'" % (dir_path, f)
                            for f in sorted(files, key=lambda x: x.lower())]
                js_modules.append("EXTRA_JS_MODULES.commonjs%s += [\n    %s,\n]\n"
                                  % (module_path, ',\n    '.join(filelist)))
            stringified = '\n'.join(js_modules)
            # This isn't the same thing as |js_modules|, since |js_modules| had
            # embedded newlines.
            lines = stringified.split('\n')
            # Indent lines while avoiding trailing whitespace.
            lines = [indent + line if line else line for line in lines]
            return '\n'.join(lines)

        moz_build_output = mozpath.join(addon_sdk_dir, 'moz.build')
        moz_build_template = mozpath.join(addon_sdk_dir, 'mozbuild.template')
        with open(moz_build_output, 'w') as f, open(moz_build_template, 'r') as t:
            substs = { 'moz-build-template': t.read(),
                       'non-b2g-modules': list_to_js_modules(non_b2g_modules,
                                                             indent='    '),
                       'always-on-modules': list_to_js_modules(always_on_modules) }
            f.write(moz_build % substs)
