# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Implicit variables; perhaps in the future this will also include some implicit
rules, at least match-anything cancellation rules.
"""

variables = {
    'MKDIR': '%pymake.builtins mkdir',
    'RM': '%pymake.builtins rm -f',
    'SLEEP': '%pymake.builtins sleep',
    'TOUCH': '%pymake.builtins touch',
    '.LIBPATTERNS': 'lib%.so lib%.a',
    '.PYMAKE': '1',
    }

