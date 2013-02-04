# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Program used to generate /packages/api-utils/lib/l10n/plural-rules.js
# Fetch unicode.org data in order to build functions specific to each language
# that will return for a given integer, its plural form name.
# Plural form names are: zero, one, two, few, many, other.
#
# More information here:
#   http://unicode.org/repos/cldr-tmp/trunk/diff/supplemental/language_plural_rules.html
#   http://cldr.unicode.org/index/cldr-spec/plural-rules

# Usage:
# $ python plural-rules-generator.py > ../packages/api-utils/lib/l10n/plural-rules.js

import urllib2
import xml.dom.minidom
import json
import re

PRINT_CONDITIONS_IN_COMMENTS = False

UNICODE_ORG_XML_URL = "http://unicode.org/repos/cldr/trunk/common/supplemental/plurals.xml"

CONDITION_RE = r'n( mod \d+)? (is|in|within|(not in))( not)? ([^\s]+)'

# For a given regexp.MatchObject `g` for `CONDITION_RE`, 
# returns the equivalent JS piece of code
# i.e. maps pseudo conditional language from unicode.org XML to JS code
def parseCondition(g):
    lvalue = "n"
    if g.group(1):
        lvalue = "(n %% %d)" % int(g.group(1).replace("mod ", ""))

    operator = g.group(2)
    if g.group(4):
        operator += " not"

    rvalue = g.group(5)

    if operator == "is":
        return "%s == %s" % (lvalue, rvalue)
    if operator == "is not":
        return "%s != %s" % (lvalue, rvalue)

    # "in", "within" or "not in" case:
    notPrefix = ""
    if operator == "not in":
        notPrefix = "!"

    # `rvalue` is a comma seperated list of either:
    #  - numbers: 42
    #  - ranges: 42..72
    sections = rvalue.split(',')

    if ".." not in rvalue:
        # If we don't have range, but only a list of integer,
        # we can simplify the generated code by using `isIn`
        # n in 1,3,6,42
        return "%sisIn(%s, [%s])" % (notPrefix, lvalue, ", ".join(sections))

    # n in 1..42
    # n in 1..3,42
    subCondition = []
    integers = []
    for sub in sections:
        if ".." in sub:
            left, right = sub.split("..")
            subCondition.append("isBetween(%s, %d, %d)" % (
                                lvalue,
                                int(left),
                                int(right)
                               ))
        else:
            integers.append(int(sub))
    if len(integers) > 1:
      subCondition.append("isIn(%s, [%s])" % (lvalue, ", ".join(integers)))
    elif len(integers) == 1:
      subCondition.append("(%s == %s)" % (lvalue, integers[0]))
    return "%s(%s)" % (notPrefix, " || ".join(subCondition))

def computeRules():
    # Fetch plural rules data directly from unicode.org website:
    url = UNICODE_ORG_XML_URL
    f = urllib2.urlopen(url)
    doc = xml.dom.minidom.parse(f)

    # Read XML document and extract locale to rules mapping
    localesMapping = {}
    algorithms = {}
    for index,pluralRules in enumerate(doc.getElementsByTagName("pluralRules")):
        if not index in algorithms:
            algorithms[index] = {}
        for locale in pluralRules.getAttribute("locales").split():
            localesMapping[locale] = index
        for rule in pluralRules.childNodes:
            if rule.nodeType != rule.ELEMENT_NODE or rule.tagName != "pluralRule":
                continue
            pluralForm = rule.getAttribute("count")
            algorithm = rule.firstChild.nodeValue
            algorithms[index][pluralForm] = algorithm

    # Go through all rules and compute a Javascript code for each of them
    rules = {}
    for index,rule in algorithms.iteritems():
        lines = []
        for pluralForm in rule:
            condition = rule[pluralForm]
            originalCondition = str(condition)

            # Convert pseudo language to JS code
            condition = rule[pluralForm].lower()
            condition = re.sub(CONDITION_RE, parseCondition, condition)
            condition = re.sub(r'or', "||", condition)
            condition = re.sub(r'and', "&&", condition)

            # Prints original condition in unicode.org pseudo language
            if PRINT_CONDITIONS_IN_COMMENTS:
                lines.append( '// %s' % originalCondition )

            lines.append( 'if (%s)' % condition )
            lines.append( '  return "%s";' % pluralForm )
            
        rules[index] = "\n    ".join(lines)
    return localesMapping, rules


localesMapping, rules = computeRules()

rulesLines = []
for index in rules:
    lines = rules[index]
    rulesLines.append('"%d": function (n) {' % index)
    rulesLines.append('  %s' % lines)
    rulesLines.append('  return "other"')
    rulesLines.append('},')

print """/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is automatically generated with /python-lib/plural-rules-generator.py
// Fetching data from: %s

// Mapping of short locale name == to == > rule index in following list
const LOCALES_TO_RULES = %s;

// Utility functions for plural rules methods
function isIn(n, list) list.indexOf(n) !== -1;
function isBetween(n, start, end) start <= n && n <= end;

// List of all plural rules methods, that maps an integer to the plural form name to use
const RULES = {
  %s
};

/**
  * Return a function that gives the plural form name for a given integer
  * for the specified `locale`
  *   let fun = getRulesForLocale('en');
  *   fun(1)    -> 'one'
  *   fun(0)    -> 'other'
  *   fun(1000) -> 'other'
  */
exports.getRulesForLocale = function getRulesForLocale(locale) {
  let index = LOCALES_TO_RULES[locale];
  if (!(index in RULES)) {
    console.warn('Plural form unknown for locale "' + locale + '"');
    return function () { return "other"; };
  }
  return RULES[index];
}
""" % (UNICODE_ORG_XML_URL,
        json.dumps(localesMapping, sort_keys=True, indent=2),
        "\n  ".join(rulesLines))
