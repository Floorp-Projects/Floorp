# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Generate bookmarks.html by interpolating the included (localized)
# `bookmarks.inc` file into the given (unlocalized) `bookmarks.html.in` input.

from __future__ import absolute_import, unicode_literals, print_function

import buildconfig
import sys

from mozbuild import preprocessor


def main(output, bookmarks_html_in, bookmarks_inc, locale=None):
    if not locale:
        raise ValueError('locale must be specified!')

    CONFIG = buildconfig.substs

    # Based on
    # https://dxr.mozilla.org/l10n-central/search?q=path%3Abookmarks.inc+%23if&redirect=true,
    # no localized input uses the preprocessor conditional #if (really,
    # anything but #define), so it's safe to restrict the set of defines to
    # what's used in mozilla-central directly.
    defines = {}
    defines['AB_CD'] = locale
    if defines['AB_CD'] == 'ja-JP-mac':
        defines['AB_CD'] = 'ja'

    defines['NIGHTLY_BUILD'] = CONFIG['NIGHTLY_BUILD']
    defines['BOOKMARKS_INCLUDE_PATH'] = bookmarks_inc

    includes = preprocessor.preprocess(includes=[bookmarks_html_in],
                                       defines=defines,
                                       output=output)
    return includes


if __name__ == "__main__":
    main(sys.argv[1:])
