# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import re

from datetime import datetime


here = os.path.abspath(os.path.dirname(__file__))
mozilla_dir = os.path.normpath(os.path.join(here, '..', '..'))

import mdn_theme

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.graphviz',
    'sphinx.ext.todo',
    'mozbuild.sphinx',
]

templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
project = u'Mozilla Build System'
year = datetime.now().year

# Grab the version from the source tree's milestone.
with open(os.path.join(mozilla_dir, 'config', 'milestone.txt'), 'rt') as fh:
    for line in fh:
        line = line.strip()

        if not line or line.startswith('#'):
            continue

        release = line
        break

version = re.sub(r'[ab]\d+$', '', release)

exclude_patterns = ['_build']
pygments_style = 'sphinx'

html_theme_path = [mdn_theme.get_theme_dir()]
html_theme = 'mdn'

html_static_path = ['_static']
htmlhelp_basename = 'MozillaBuildSystemdoc'
