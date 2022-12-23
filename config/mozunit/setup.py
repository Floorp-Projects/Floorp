# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

setup(
    name="mozunit",
    version="1.0",
    description="Make unit tests report the way Mozilla infrastructure expects",
    classifiers=["Programming Language :: Python :: 2.7"],
    keywords="mozilla",
    author="Mozilla Automation and Tools team",
    author_email="tools@lists.mozilla.org",
    license="MPL",
    packages=["mozunit"],
    include_package_data=True,
    zip_safe=False,
)
