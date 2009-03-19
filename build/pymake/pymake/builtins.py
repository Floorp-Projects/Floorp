"""
Implicit variables; perhaps in the future this will also include some implicit
rules, at least match-anything cancellation rules.
"""

variables = {
    'RM': 'rm -f',
    '.LIBPATTERNS': 'lib%.so lib%.a',
    '.PYMAKE': '1',
    }
