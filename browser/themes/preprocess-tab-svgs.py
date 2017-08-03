#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig

from mozbuild.preprocessor import preprocess

# By default, the pre-processor used for jar.mn will use "%" as a marker
# for ".css" files and "#" otherwise. This falls apart when a file using
# one marker needs to include a file with the other marker since the
# pre-processor instructions in the included file will not be
# processed. The following SVG files need to include a file which uses
# "%" as the marker so we invoke the pre- processor ourselves here with
# the marker specified. The resulting SVG files will get packaged by the
# processing of the jar file in the appropriate directory.
def _do_preprocessing(output_svg, input_svg_file, additional_defines):
    additional_defines.update(buildconfig.defines)
    return preprocess(output=output_svg,
                      includes=[input_svg_file],
                      marker='%',
                      defines=additional_defines)

def tab_side_start(output_svg, input_svg_file):
    return _do_preprocessing(output_svg, input_svg_file, {'TAB_SIDE': 'start'})

def tab_side_end(output_svg, input_svg_file):
    return _do_preprocessing(output_svg, input_svg_file, {'TAB_SIDE': 'end'})

