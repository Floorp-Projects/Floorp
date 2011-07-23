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

