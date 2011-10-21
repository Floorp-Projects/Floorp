# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla Browser code.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Axel Hecht <l10n@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import sys
import re
import codecs
try:
  from Mozilla.Parser import getParser
except ImportError:
  sys.exit('''extract-bookmarks needs compare-locales

Find that on http://pypi.python.org/pypi/compare-locales.
This script has been tested with version 0.6, and might work with future 
versions.''')

ll=re.compile('\.(title|a|dd|h[0-9])$')

p = getParser(sys.argv[1])
p.readFile(sys.argv[1])

template = '''#filter emptyLines

# LOCALIZATION NOTE: The 'en-US' strings in the URLs will be replaced with
# your locale code, and link to your translated pages as soon as they're 
# live.

#define bookmarks_title %s
#define bookmarks_heading %s

#define bookmarks_toolbarfolder %s
#define bookmarks_toolbarfolder_description %s

# LOCALIZATION NOTE (getting_started):
# link title for http://www.mozilla.com/en-US/firefox/central/
#define getting_started %s

# LOCALIZATION NOTE (firefox_heading):
# Firefox links folder name
#define firefox_heading %s

# LOCALIZATION NOTE (firefox_help):
# link title for http://www.mozilla.com/en-US/firefox/help/
#define firefox_help %s

# LOCALIZATION NOTE (firefox_customize):
# link title for http://www.mozilla.com/en-US/firefox/customize/
#define firefox_customize %s

# LOCALIZATION NOTE (firefox_community):
# link title for http://www.mozilla.com/en-US/firefox/community/
#define firefox_community %s

# LOCALIZATION NOTE (firefox_about):
# link title for http://www.mozilla.com/en-US/about/
#define firefox_about %s

#unfilter emptyLines'''

strings = tuple(e.val for e in p if ll.search(e.key))

print codecs.utf_8_encode(template % strings)[0]
