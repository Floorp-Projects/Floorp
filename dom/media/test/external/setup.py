# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from setuptools import setup, find_packages

PACKAGE_VERSION = '2.0'

THIS_DIR = os.path.dirname(os.path.realpath(__name__))


def read(*parts):
    with open(os.path.join(THIS_DIR, *parts)) as f:
        return f.read()

setup(name='external-media-tests',
      version=PACKAGE_VERSION,
      description=('A collection of Mozilla Firefox media playback tests run '
                   'with Marionette'),
      classifiers=[
          'Environment :: Console',
          'Intended Audience :: Developers',
          'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
          'Natural Language :: English',
          'Operating System :: OS Independent',
          'Programming Language :: Python',
          'Topic :: Software Development :: Libraries :: Python Modules',
      ],
      keywords='mozilla',
      author='Mozilla Automation and Tools Team',
      author_email='tools@lists.mozilla.org',
      url='https://hg.mozilla.org/mozilla-central/dom/media/test/external/',
      license='MPL 2.0',
      packages=find_packages(),
      zip_safe=False,
      install_requires=read('requirements.txt').splitlines(),
      include_package_data=True,
      entry_points="""
        [console_scripts]
        external-media-tests = external_media_harness:cli
    """)
