# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup, find_packages

PACKAGE_VERSION = '0.4'

deps = [
    'marionette-client == 2.0.0',
    'marionette-driver == 1.1.1',
    'mozlog == 3.0',
    'manifestparser == 1.1',
    'firefox-puppeteer >= 3.1.0, <4.0.0',
]

setup(name='firefox-media-tests',
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
      url='https://github.com/mjzffr/firefox-media-tests',
      license='MPL 2.0',
      packages=find_packages(),
      zip_safe=False,
      install_requires=deps,
      include_package_data=True,
      entry_points="""
        [console_scripts]
        firefox-media-tests = harness:cli
    """)
