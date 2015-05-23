# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig
import collections
import re
import StringIO
import sys

from mozbuild.preprocessor import preprocess

def read_conf(conf_filename):
    # Invoking the preprocessor ourselves is easier than writing build rules
    # to do it for us.
    processed = StringIO.StringIO()
    preprocess(includes=[conf_filename],
               defines=buildconfig.defines,
               output=processed)

    # Can't read/write from a single StringIO, so make a new one for reading.
    stream = StringIO.StringIO(processed.getvalue())

    def parse_counters(stream):
        for line_num, line in enumerate(stream):
            line = line.rstrip('\n')
            if not line or line.startswith('//'):
                # empty line or comment
                continue
            m = re.match(r'method ([A-Za-z0-9]+)\.([A-Za-z0-9]+)$', line)
            if m:
                interface_name, method_name = m.groups()
                yield { 'type': 'method',
                        'interface_name': interface_name,
                        'method_name': method_name }
                continue
            m = re.match(r'attribute ([A-Za-z0-9]+)\.([A-Za-z0-9]+)$', line)
            if m:
                interface_name, attribute_name = m.groups()
                yield { 'type': 'attribute',
                        'interface_name': interface_name,
                        'attribute_name': attribute_name }
                continue
            m = re.match(r'property ([a-z0-9-]+)$', line)
            if m:
                property_name = m.group(1)
                yield { 'type': 'property',
                        'property_name': property_name }
                continue
            raise ValueError('error parsing %s at line %d' % (conf_filename, line_num))

    return parse_counters(stream)

def generate_histograms(filename):
    # The mapping for use counters to telemetry histograms depends on the
    # ordering of items in the dictionary.
    items = collections.OrderedDict()
    for counter in read_conf(filename):
        def append_counter(name, desc):
            items[name] = { 'expires_in_version': 'never',
                            'kind' : 'boolean',
                            'description': desc }

        def append_counters(name, desc):
            append_counter('USE_COUNTER_%s_DOCUMENT' % name, 'Whether a document %s' % desc)
            append_counter('USE_COUNTER_%s_PAGE' % name, 'Whether a page %s' % desc)

        if counter['type'] == 'method':
            method = '%s.%s' % (counter['interface_name'], counter['method_name'])
            append_counters(method.replace('.', '_').upper(), 'called %s' % method)
        elif counter['type'] == 'attribute':
            attr = '%s.%s' % (counter['interface_name'], counter['attribute_name'])
            append_counters(attr.replace('.', '_').upper(), 'got or set %s' % attr)
        elif counter['type'] == 'property':
            prop = counter['property_name']
            append_counters('PROPERTY_%s' % prop.replace('-', '_').upper(), "used the '%s' property" % prop)

    return items
