#! /bin/sh
#
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

# allmakefiles.sh - List of all makefiles. 
#   Appends the list of makefiles to the variable, MAKEFILES.
#   There is no need to rerun autoconf after adding makefiles.
#   You only need to run configure.
#
#   Please keep the modules in this file in sync with those in
#   mozilla/build/unix/modules.mk
#

MAKEFILES=""

# add_makefiles - Shell function to add makefiles to MAKEFILES
add_makefiles() {
    MAKEFILES="$MAKEFILES $*"
}

if [ "$srcdir" = "" ]; then
    srcdir=.
fi

#
# Common makefiles used by everyone
#
add_makefiles "
Makefile
build/Makefile
build/unix/Makefile
config/Makefile
config/autoconf.mk
config/mkdepend/Makefile
config/doxygen.cfg
"

if [ "$MOZ_COMPOSER" ]; then
MAKEFILES_composer="
editor/composer/Makefile
editor/ui/Makefile
editor/ui/locales/Makefile
"
fi

MAKEFILES_db="
db/Makefile
db/mdb/Makefile
db/mdb/public/Makefile
db/mork/Makefile
db/mork/build/Makefile
db/mork/src/Makefile
"

MAKEFILES_storage="
db/sqlite3/src/Makefile
db/morkreader/Makefile
storage/Makefile
storage/public/Makefile
storage/src/Makefile
storage/build/Makefile
storage/test/Makefile
"

MAKEFILES_dom="
dom/Makefile
dom/public/Makefile
dom/public/base/Makefile
dom/public/coreEvents/Makefile
dom/public/idl/Makefile
dom/public/idl/base/Makefile
dom/public/idl/canvas/Makefile
dom/public/idl/core/Makefile
dom/public/idl/css/Makefile
dom/public/idl/events/Makefile
dom/public/idl/html/Makefile
dom/public/idl/range/Makefile
dom/public/idl/stylesheets/Makefile
dom/public/idl/views/Makefile
dom/public/idl/xbl/Makefile
dom/public/idl/xpath/Makefile
dom/public/idl/xul/Makefile
dom/src/Makefile
dom/src/base/Makefile
dom/src/events/Makefile
dom/src/jsurl/Makefile
dom/locales/Makefile
"

MAKEFILES_editor="
editor/Makefile
editor/public/Makefile
editor/idl/Makefile
editor/txmgr/Makefile
editor/txmgr/idl/Makefile
editor/txmgr/public/Makefile
editor/txmgr/src/Makefile
editor/txmgr/tests/Makefile
editor/txtsvc/Makefile
editor/txtsvc/public/Makefile
editor/txtsvc/src/Makefile
"

MAKEFILES_expat="
parser/expat/Makefile
parser/expat/lib/Makefile
"

MAKEFILES_extensions="
extensions/Makefile
"

MAKEFILES_gc="
gc/boehm/Makefile
gc/boehm/leaksoup/Makefile
"

MAKEFILES_gfx="
gfx/Makefile
gfx/idl/Makefile
gfx/public/Makefile
gfx/src/Makefile
gfx/src/beos/Makefile
gfx/src/gtk/Makefile
gfx/src/ps/Makefile
gfx/src/psshared/Makefile
gfx/src/photon/Makefile
gfx/src/mac/Makefile
gfx/src/qt/Makefile
gfx/src/xlib/Makefile
gfx/src/os2/Makefile
gfx/src/xlibrgb/Makefile
gfx/src/windows/Makefile
gfx/src/thebes/Makefile
gfx/tests/Makefile
"

if [ "$MOZ_TREE_CAIRO" ] ; then
MAKEFILES_gfx="$MAKEFILES_gfx
gfx/cairo/Makefile
gfx/cairo/libpixman/src/Makefile
gfx/cairo/cairo/src/Makefile
gfx/cairo/cairo/src/cairo-features.h
gfx/cairo/glitz/src/Makefile
gfx/cairo/glitz/src/glx/Makefile
gfx/cairo/glitz/src/wgl/Makefile
"
fi

MAKEFILES_htmlparser="
parser/htmlparser/Makefile
parser/htmlparser/robot/Makefile
parser/htmlparser/robot/test/Makefile
parser/htmlparser/public/Makefile
parser/htmlparser/src/Makefile
parser/htmlparser/tests/Makefile
parser/htmlparser/tests/grabpage/Makefile
parser/htmlparser/tests/logparse/Makefile
parser/htmlparser/tests/html/Makefile
parser/htmlparser/tests/outsinks/Makefile
"

MAKEFILES_intl="
intl/Makefile
intl/chardet/Makefile
intl/chardet/public/Makefile
intl/chardet/src/Makefile
intl/uconv/Makefile
intl/uconv/idl/Makefile
intl/uconv/public/Makefile
intl/uconv/src/Makefile
intl/uconv/tests/Makefile
intl/uconv/ucvja/Makefile
intl/uconv/ucvlatin/Makefile
intl/uconv/ucvcn/Makefile
intl/uconv/ucvtw/Makefile
intl/uconv/ucvtw2/Makefile
intl/uconv/ucvko/Makefile
intl/uconv/ucvibm/Makefile
intl/uconv/native/Makefile
intl/locale/Makefile
intl/locale/public/Makefile
intl/locale/idl/Makefile
intl/locale/src/Makefile
intl/locale/src/unix/Makefile
intl/locale/src/os2/Makefile
intl/locale/src/windows/Makefile
intl/locale/tests/Makefile
intl/lwbrk/Makefile
intl/lwbrk/src/Makefile
intl/lwbrk/public/Makefile
intl/lwbrk/tests/Makefile
intl/unicharutil/Makefile
intl/unicharutil/util/Makefile
intl/unicharutil/util/internal/Makefile
intl/unicharutil/idl/Makefile
intl/unicharutil/src/Makefile
intl/unicharutil/public/Makefile
intl/unicharutil/tables/Makefile
intl/unicharutil/tests/Makefile
intl/unicharutil/tools/Makefile
intl/strres/Makefile
intl/strres/public/Makefile
intl/strres/src/Makefile
intl/strres/tests/Makefile
"

if [ "$SUNCTL" ] ; then
MAKEFILES_intl="$MAKEFILES_intl
intl/ctl/Makefile
intl/ctl/public/Makefile
intl/ctl/src/Makefile
intl/ctl/src/pangoLite/Makefile
intl/ctl/src/thaiShaper/Makefile
intl/ctl/src/hindiShaper/Makefile
"
fi

if [ "$MOZ_UNIVERSALCHARDET" ] ; then
MAKEFILES_intl="$MAKEFILES_intl
extensions/universalchardet/Makefile
extensions/universalchardet/src/Makefile
extensions/universalchardet/src/base/Makefile
extensions/universalchardet/src/xpcom/Makefile
extensions/universalchardet/tests/Makefile
"
fi

MAKEFILES_js="
js/src/Makefile
js/src/fdlibm/Makefile
"

MAKEFILES_liveconnect="
js/src/liveconnect/Makefile
js/src/liveconnect/classes/Makefile
"

MAKEFILES_xpconnect="
js/src/xpconnect/Makefile
js/src/xpconnect/public/Makefile
js/src/xpconnect/idl/Makefile
js/src/xpconnect/shell/Makefile
js/src/xpconnect/src/Makefile
js/src/xpconnect/loader/Makefile
js/src/xpconnect/tests/Makefile
js/src/xpconnect/tests/components/Makefile
js/src/xpconnect/tests/idl/Makefile
js/src/xpconnect/shell/Makefile
js/src/xpconnect/tools/Makefile
js/src/xpconnect/tools/idl/Makefile
js/src/xpconnect/tools/idl/Makefile
"

MAKEFILES_jsdebugger="
js/jsd/Makefile
js/jsd/idl/Makefile
"

MAKEFILES_content="
content/Makefile
content/base/Makefile
content/base/public/Makefile
content/base/src/Makefile
content/canvas/Makefile
content/canvas/public/Makefile
content/canvas/src/Makefile
content/events/Makefile
content/events/public/Makefile
content/events/src/Makefile
content/html/Makefile
content/html/content/Makefile
content/html/content/public/Makefile
content/html/content/src/Makefile
content/html/document/Makefile
content/html/document/public/Makefile
content/html/document/src/Makefile
content/xml/Makefile
content/xml/content/Makefile
content/xml/content/public/Makefile
content/xml/content/src/Makefile
content/xml/document/Makefile
content/xml/document/public/Makefile
content/xml/document/src/Makefile
content/xul/Makefile
content/xul/content/Makefile
content/xul/content/public/Makefile
content/xul/content/src/Makefile
content/xul/document/Makefile
content/xul/document/public/Makefile
content/xul/document/src/Makefile
content/xul/templates/public/Makefile
content/xul/templates/src/Makefile
content/xbl/Makefile
content/xbl/public/Makefile
content/xbl/src/Makefile
content/xbl/builtin/Makefile
content/xslt/Makefile
content/xslt/public/Makefile
content/xslt/src/Makefile
content/xslt/src/base/Makefile
content/xslt/src/xml/Makefile
content/xslt/src/xpath/Makefile
content/xslt/src/xslt/Makefile
content/xslt/src/main/Makefile
"

MAKEFILES_layout="
layout/Makefile
layout/base/Makefile
layout/base/tests/Makefile
layout/build/Makefile
layout/forms/Makefile
layout/html/tests/Makefile
layout/style/Makefile
layout/printing/Makefile
layout/tools/Makefile
layout/xul/Makefile
layout/xul/base/Makefile
layout/xul/base/public/Makefile
layout/xul/base/src/Makefile
layout/xul/base/src/tree/Makefile
layout/xul/base/src/tree/src/Makefile
layout/xul/base/src/tree/public/Makefile
"

MAKEFILES_libimg="
modules/libimg/Makefile
"

MAKEFILES_libjar="
modules/libjar/Makefile
modules/libjar/standalone/Makefile
modules/libjar/test/Makefile
"

MAKEFILES_libreg="
modules/libreg/Makefile
modules/libreg/include/Makefile
modules/libreg/src/Makefile
modules/libreg/standalone/Makefile
"

MAKEFILES_libpref="
modules/libpref/Makefile
modules/libpref/public/Makefile
modules/libpref/src/Makefile
"

MAKEFILES_libutil="
modules/libutil/Makefile
modules/libutil/public/Makefile
modules/libutil/src/Makefile
"

MAKEFILES_oji="
modules/oji/Makefile
modules/oji/public/Makefile
modules/oji/src/Makefile
plugin/oji/JEP/Makefile
"

MAKEFILES_plugin="
modules/plugin/Makefile
modules/plugin/base/src/Makefile
modules/plugin/base/public/Makefile
modules/plugin/samples/simple/Makefile
modules/plugin/samples/SanePlugin/Makefile
modules/plugin/samples/default/unix/Makefile
modules/plugin/tools/sdk/Makefile
modules/plugin/tools/sdk/samples/Makefile
modules/plugin/tools/sdk/samples/common/Makefile
modules/plugin/tools/sdk/samples/basic/windows/Makefile
modules/plugin/tools/sdk/samples/scriptable/windows/Makefile
modules/plugin/tools/sdk/samples/simple/Makefile
modules/plugin/tools/sdk/samples/winless/windows/Makefile
"

MAKEFILES_access_builtin="
extensions/access-builtin/Makefile
extensions/access-builtin/accessproxy/Makefile
"

MAKEFILES_netwerk="
netwerk/Makefile
netwerk/base/Makefile
netwerk/base/public/Makefile
netwerk/base/src/Makefile
netwerk/build/Makefile
netwerk/cache/Makefile
netwerk/cache/public/Makefile
netwerk/cache/src/Makefile
netwerk/cookie/Makefile
netwerk/cookie/public/Makefile
netwerk/cookie/src/Makefile
netwerk/dns/Makefile
netwerk/dns/public/Makefile
netwerk/dns/src/Makefile
netwerk/protocol/Makefile
netwerk/protocol/about/Makefile
netwerk/protocol/about/public/Makefile
netwerk/protocol/about/src/Makefile
netwerk/protocol/data/Makefile
netwerk/protocol/data/src/Makefile
netwerk/protocol/file/Makefile
netwerk/protocol/file/public/Makefile
netwerk/protocol/file/src/Makefile
netwerk/protocol/ftp/Makefile
netwerk/protocol/ftp/public/Makefile
netwerk/protocol/ftp/src/Makefile
netwerk/protocol/gopher/Makefile
netwerk/protocol/gopher/src/Makefile
netwerk/protocol/http/Makefile
netwerk/protocol/http/public/Makefile
netwerk/protocol/http/src/Makefile
netwerk/protocol/res/Makefile
netwerk/protocol/res/public/Makefile
netwerk/protocol/res/src/Makefile
netwerk/mime/Makefile
netwerk/mime/public/Makefile
netwerk/mime/src/Makefile
netwerk/socket/Makefile
netwerk/socket/base/Makefile
netwerk/streamconv/Makefile
netwerk/streamconv/converters/Makefile
netwerk/streamconv/public/Makefile
netwerk/streamconv/src/Makefile
netwerk/streamconv/test/Makefile
netwerk/test/Makefile
netwerk/testserver/Makefile
netwerk/resources/Makefile
netwerk/locales/Makefile
netwerk/system/Makefile
netwerk/system/win32/Makefile
uriloader/exthandler/Makefile
intl/strres/public/Makefile
intl/locale/idl/Makefile
$MAKEFILES_js
modules/libpref/public/Makefile
"

if [ "$MOZ_AUTH_EXTENSION" ]; then
    MAKEFILES_netwerk="$MAKEFILES_netwerk
        extensions/auth/Makefile
    "
fi

MAKEFILES_uriloader="
uriloader/Makefile
uriloader/base/Makefile
uriloader/exthandler/Makefile
"

MAKEFILES_profile="
profile/Makefile
profile/src/Makefile
profile/public/Makefile
profile/resources/Makefile
profile/pref-migrator/Makefile
profile/pref-migrator/public/Makefile
profile/pref-migrator/src/Makefile
profile/pref-migrator/resources/Makefile
profile/defaults/Makefile
profile/dirserviceprovider/Makefile
profile/dirserviceprovider/public/Makefile
profile/dirserviceprovider/src/Makefile
"

MAKEFILES_rdf="
rdf/Makefile
rdf/base/Makefile
rdf/base/idl/Makefile
rdf/base/public/Makefile
rdf/base/src/Makefile
rdf/util/Makefile
rdf/util/public/Makefile
rdf/util/src/Makefile
rdf/build/Makefile
rdf/datasource/Makefile
rdf/datasource/public/Makefile
rdf/datasource/src/Makefile
rdf/tests/Makefile
rdf/tests/rdfcat/Makefile
rdf/tests/rdfpoll/Makefile
"

MAKEFILES_sun_java="
sun-java/Makefile
sun-java/stubs/Makefile
sun-java/stubs/include/Makefile
sun-java/stubs/jri/Makefile
"

MAKEFILES_caps="
caps/Makefile
caps/idl/Makefile
caps/include/Makefile
caps/src/Makefile
"

MAKEFILES_chrome="
chrome/Makefile
chrome/public/Makefile
chrome/src/Makefile
embedding/minimo/chromelite/Makefile
rdf/chrome/Makefile
rdf/chrome/public/Makefile
rdf/chrome/build/Makefile
rdf/chrome/src/Makefile
rdf/chrome/tools/Makefile
rdf/chrome/tools/chromereg/Makefile
"


MAKEFILES_view="
view/Makefile
view/public/Makefile
view/src/Makefile
"

MAKEFILES_docshell="
docshell/Makefile
docshell/base/Makefile
docshell/shistory/Makefile
docshell/shistory/public/Makefile
docshell/shistory/src/Makefile
docshell/build/Makefile
"

MAKEFILES_webshell="
webshell/Makefile
webshell/public/Makefile
"

MAKEFILES_widget="
widget/Makefile
widget/public/Makefile
widget/src/Makefile
widget/src/beos/Makefile
widget/src/build/Makefile
widget/src/gtk/Makefile
widget/src/gtksuperwin/Makefile
widget/src/gtkxtbin/Makefile
widget/src/qt/Makefile
widget/src/photon/Makefile
widget/src/mac/Makefile
widget/src/cocoa/Makefile
widget/src/xlib/Makefile
widget/src/os2/Makefile
widget/src/windows/Makefile
widget/src/xlibxtbin/Makefile
widget/src/xpwidgets/Makefile
widget/src/support/Makefile
"

MAKEFILES_xpcom="
xpcom/string/Makefile
xpcom/string/public/Makefile
xpcom/string/src/Makefile
xpcom/Makefile
xpcom/base/Makefile
xpcom/build/Makefile
xpcom/components/Makefile
xpcom/ds/Makefile
xpcom/glue/Makefile
xpcom/glue/standalone/Makefile
xpcom/io/Makefile
xpcom/typelib/Makefile
xpcom/reflect/Makefile
xpcom/typelib/xpt/Makefile
xpcom/typelib/xpt/public/Makefile
xpcom/typelib/xpt/src/Makefile
xpcom/typelib/xpt/tests/Makefile
xpcom/typelib/xpt/tools/Makefile
xpcom/typelib/xpidl/Makefile
xpcom/reflect/xptcall/Makefile
xpcom/reflect/xptcall/public/Makefile
xpcom/reflect/xptcall/src/Makefile
xpcom/reflect/xptcall/src/md/Makefile
xpcom/reflect/xptcall/src/md/os2/Makefile
xpcom/reflect/xptcall/src/md/test/Makefile
xpcom/reflect/xptcall/src/md/unix/Makefile
xpcom/reflect/xptcall/src/md/win32/Makefile
xpcom/reflect/xptcall/tests/Makefile
xpcom/reflect/xptinfo/Makefile
xpcom/reflect/xptinfo/public/Makefile
xpcom/reflect/xptinfo/src/Makefile
xpcom/reflect/xptinfo/tests/Makefile
xpcom/proxy/Makefile
xpcom/proxy/public/Makefile
xpcom/proxy/src/Makefile
xpcom/proxy/tests/Makefile
xpcom/sample/Makefile
xpcom/threads/Makefile
xpcom/tools/Makefile
xpcom/tools/registry/Makefile
xpcom/stub/Makefile
xpcom/windbgdlg/Makefile
xpcom/system/Makefile
$MAKEFILES_libreg
$MAKEFILES_libjar
intl/unicharutil/public/Makefile
intl/uconv/public/Makefile
netwerk/base/public/Makefile
netwerk/build/Makefile
"
MAKEFILES_xpcom_obsolete="
xpcom/obsolete/Makefile
xpcom/obsolete/component/Makefile
"

MAKEFILES_xpcom_tests="
xpcom/tests/Makefile
xpcom/tests/dynamic/Makefile
xpcom/tests/services/Makefile
xpcom/tests/windows/Makefile
"

MAKEFILES_string="$MAKEFILES_xpcom"

MAKEFILES_xpinstall="
xpinstall/Makefile
xpinstall/packager/Makefile
xpinstall/packager/unix/Makefile
xpinstall/packager/windows/Makefile
xpinstall/packager/os2/Makefile
xpinstall/public/Makefile
xpinstall/res/Makefile
xpinstall/src/Makefile
xpinstall/stub/Makefile
xpinstall/wizard/libxpnet/Makefile
xpinstall/wizard/libxpnet/src/Makefile
xpinstall/wizard/libxpnet/test/Makefile
xpinstall/wizard/unix/src2/Makefile
xpinstall/wizard/windows/builder/Makefile
xpinstall/wizard/windows/nsinstall/Makefile
xpinstall/wizard/windows/nsztool/Makefile
xpinstall/wizard/windows/uninstall/Makefile
xpinstall/wizard/windows/setup/Makefile
xpinstall/wizard/windows/setuprsc/Makefile
xpinstall/wizard/windows/ren8dot3/Makefile
xpinstall/wizard/windows/ds32/Makefile
xpinstall/wizard/windows/GetShortPathName/Makefile
"

MAKEFILES_xpfe="
widget/src/xremoteclient/Makefile
toolkit/components/Makefile
toolkit/components/remote/Makefile
xpfe/Makefile
xpfe/browser/Makefile
xpfe/browser/public/Makefile
xpfe/browser/src/Makefile
xpfe/components/Makefile
xpfe/components/bookmarks/Makefile
xpfe/components/bookmarks/public/Makefile
xpfe/components/bookmarks/src/Makefile
xpfe/components/bookmarks/resources/Makefile
xpfe/components/directory/Makefile
xpfe/components/download-manager/Makefile
xpfe/components/download-manager/src/Makefile
xpfe/components/download-manager/public/Makefile
xpfe/components/download-manager/resources/Makefile
xpfe/components/extensions/Makefile
xpfe/components/extensions/src/Makefile
xpfe/components/extensions/public/Makefile
xpfe/components/find/Makefile
xpfe/components/find/public/Makefile
xpfe/components/find/src/Makefile
xpfe/components/filepicker/Makefile
xpfe/components/filepicker/public/Makefile
xpfe/components/filepicker/src/Makefile
xpfe/components/history/Makefile
xpfe/components/history/src/Makefile
xpfe/components/history/public/Makefile
xpfe/components/intl/Makefile
xpfe/components/related/Makefile
xpfe/components/related/src/Makefile
xpfe/components/related/public/Makefile
xpfe/components/search/Makefile
xpfe/components/search/datasets/Makefile
xpfe/components/search/public/Makefile
xpfe/components/search/src/Makefile
xpfe/components/sidebar/Makefile
xpfe/components/sidebar/src/Makefile
xpfe/components/startup/Makefile
xpfe/components/startup/public/Makefile
xpfe/components/startup/src/Makefile
xpfe/components/autocomplete/Makefile
xpfe/components/autocomplete/public/Makefile
xpfe/components/autocomplete/src/Makefile
xpfe/components/updates/Makefile
xpfe/components/updates/src/Makefile
xpfe/components/urlwidget/Makefile
xpfe/components/winhooks/Makefile
xpfe/components/windowds/Makefile
xpfe/components/alerts/Makefile
xpfe/components/alerts/public/Makefile
xpfe/components/alerts/src/Makefile
xpfe/components/console/Makefile
xpfe/components/resetPref/Makefile
xpfe/components/killAll/Makefile
xpfe/components/build/Makefile
xpfe/components/xremote/Makefile
xpfe/components/xremote/public/Makefile
xpfe/components/xremote/src/Makefile
xpfe/appshell/Makefile
xpfe/appshell/src/Makefile
xpfe/appshell/public/Makefile
xpfe/bootstrap/Makefile
xpfe/bootstrap/init.d/Makefile
xpfe/bootstrap/appleevents/Makefile
xpfe/browser/Makefile
xpfe/browser/src/Makefile
xpfe/global/Makefile
xpfe/global/buildconfig.html
xpfe/global/resources/Makefile
xpfe/global/resources/content/Makefile
xpfe/global/resources/content/os2/Makefile
xpfe/global/resources/content/unix/Makefile
xpfe/global/resources/locale/Makefile
xpfe/global/resources/locale/en-US/Makefile
xpfe/global/resources/locale/en-US/mac/Makefile
xpfe/global/resources/locale/en-US/os2/Makefile
xpfe/global/resources/locale/en-US/unix/Makefile
xpfe/global/resources/locale/en-US/win/Makefile
xpfe/communicator/Makefile
extensions/spellcheck/Makefile
extensions/spellcheck/idl/Makefile
extensions/spellcheck/locales/Makefile
extensions/spellcheck/myspell/Makefile
extensions/spellcheck/src/Makefile
"

MAKEFILES_embedding="
embedding/Makefile
embedding/base/Makefile
embedding/browser/Makefile
embedding/browser/activex/src/Makefile
embedding/browser/activex/src/control/Makefile
embedding/browser/activex/src/control_kicker/Makefile
embedding/browser/build/Makefile
embedding/browser/chrome/Makefile
embedding/browser/webBrowser/Makefile
embedding/browser/gtk/Makefile
embedding/browser/gtk/src/Makefile
embedding/browser/gtk/tests/Makefile
embedding/browser/qt/Makefile
embedding/browser/qt/src/Makefile
embedding/browser/qt/tests/Makefile
embedding/browser/photon/Makefile
embedding/browser/photon/src/Makefile
embedding/browser/photon/tests/Makefile
embedding/browser/cocoa/Makefile
embedding/components/Makefile
embedding/components/build/Makefile
embedding/components/windowwatcher/Makefile
embedding/components/windowwatcher/public/Makefile
embedding/components/windowwatcher/src/Makefile
embedding/components/ui/Makefile
embedding/components/ui/helperAppDlg/Makefile
embedding/components/ui/progressDlg/Makefile
embedding/config/Makefile
embedding/tests/Makefile
embedding/tests/cocoaEmbed/Makefile
embedding/tests/winEmbed/Makefile
embedding/tests/mfcembed/Makefile
embedding/tests/mfcembed/components/Makefile
"

MAKEFILES_minimo="
minimo/Makefile
minimo/base/Makefile
minimo/base/wince/Makefile
minimo/components/Makefile
minimo/components/phone/Makefile
minimo/components/softkb/Makefile
minimo/components/ssr/Makefile
minimo/customization/Makefile
minimo/chrome/Makefile
"

MAKEFILES_psm2="
security/manager/Makefile
security/manager/boot/Makefile
security/manager/boot/src/Makefile
security/manager/boot/public/Makefile
security/manager/ssl/Makefile
security/manager/ssl/src/Makefile
security/manager/ssl/resources/Makefile
security/manager/ssl/public/Makefile
security/manager/pki/Makefile
security/manager/pki/resources/Makefile
security/manager/pki/src/Makefile
security/manager/pki/public/Makefile
security/manager/locales/Makefile
netwerk/protocol/http/public/Makefile
netwerk/build/Makefile
netwerk/base/public/Makefile
netwerk/socket/base/Makefile
uriloader/base/Makefile
intl/locale/idl/Makefile
intl/strres/public/Makefile
dom/public/Makefile
dom/public/base/Makefile
rdf/base/idl/Makefile
xpfe/appshell/public/Makefile
caps/idl/Makefile
gfx/public/Makefile
gfx/idl/Makefile
widget/public/Makefile
modules/libpref/public/Makefile
content/base/public/Makefile
intl/locale/public/Makefile
"

MAKEFILES_inspector="
extensions/inspector/Makefile
extensions/inspector/base/Makefile
extensions/inspector/build/Makefile
extensions/inspector/resources/Makefile
extensions/inspector/resources/locale/Makefile
"

MAKEFILES_spatialnavigation="
extensions/spatialnavigation/Makefile
extensions/spatialnavigation/public/Makefile
extensions/spatialnavigation/src/Makefile
"

MAKEFILES_sroaming="
extensions/sroaming/Makefile
extensions/sroaming/src/Makefile
"

MAKEFILES_tridentprofile="
extensions/tridentprofile/Makefile
extensions/tridentprofile/public/Makefile
extensions/tridentprofile/resources/Makefile
extensions/tridentprofile/src/Makefile
"

MAKEFILES_typeaheadfind="
extensions/typeaheadfind/public/Makefile
extensions/typeaheadfind/resources/Makefile
extensions/typeaheadfind/src/Makefile
extensions/typeaheadfind/Makefile
"

MAKEFILES_metrics="
extensions/metrics/Makefile
extensions/metrics/build/Makefile
extensions/metrics/public/Makefile
extensions/metrics/src/Makefile
extensions/metrics/test/Makefile
"

MAKEFILES_phoenix="
browser/Makefile
browser/app/Makefile
browser/app/profile/extensions/Makefile
browser/base/Makefile
browser/components/Makefile
browser/components/bookmarks/Makefile
browser/components/bookmarks/public/Makefile
browser/components/bookmarks/src/Makefile
browser/components/build/Makefile
browser/components/dirprovider/Makefile
browser/components/history/Makefile
browser/components/migration/Makefile
browser/components/migration/public/Makefile
browser/components/migration/src/Makefile
browser/components/places/Makefile
browser/components/preferences/Makefile
browser/components/search/Makefile
browser/components/sidebar/Makefile
browser/components/sidebar/src/Makefile
browser/components/shell/Makefile
browser/components/shell/public/Makefile
browser/components/shell/src/Makefile
browser/extensions/Makefile
browser/extensions/layout-debug/Makefile
browser/installer/Makefile
browser/installer/unix/Makefile
browser/installer/windows/Makefile
browser/locales/Makefile
browser/themes/Makefile
browser/themes/pinstripe/browser/Makefile
browser/themes/pinstripe/Makefile
browser/themes/winstripe/browser/Makefile
browser/themes/winstripe/Makefile
"

MAKEFILES_suite="
suite/Makefile
suite/app/Makefile
suite/branding/Makefile
suite/browser/Makefile
suite/build/Makefile
suite/common/Makefile
suite/components/Makefile
suite/components/xulappinfo/Makefile
suite/locales/Makefile
suite/profile/Makefile
suite/profile/migration/Makefile
suite/profile/migration/public/Makefile
suite/profile/migration/src/Makefile
"

MAKEFILES_xulrunner="
xulrunner/Makefile
xulrunner/app/Makefile
xulrunner/app/profile/Makefile
xulrunner/app/profile/chrome/Makefile
xulrunner/app/profile/extensions/Makefile
xulrunner/installer/Makefile
xulrunner/installer/mac/Makefile
"

MAKEFILES_xulapp="
toolkit/Makefile
toolkit/library/Makefile
toolkit/content/Makefile
toolkit/content/buildconfig.html
toolkit/obsolete/Makefile
toolkit/components/alerts/Makefile
toolkit/components/alerts/public/Makefile
toolkit/components/alerts/src/Makefile
toolkit/components/autocomplete/Makefile
toolkit/components/autocomplete/public/Makefile
toolkit/components/autocomplete/src/Makefile
toolkit/components/Makefile
toolkit/components/build/Makefile
toolkit/components/commandlines/Makefile
toolkit/components/commandlines/public/Makefile
toolkit/components/commandlines/src/Makefile
toolkit/components/console/Makefile
toolkit/components/cookie/Makefile
toolkit/components/downloads/public/Makefile
toolkit/components/downloads/Makefile
toolkit/components/downloads/src/Makefile
toolkit/components/filepicker/Makefile
toolkit/system/gnome/Makefile
toolkit/components/help/Makefile
toolkit/components/history/Makefile
toolkit/components/history/public/Makefile
toolkit/components/history/src/Makefile
toolkit/components/passwordmgr/Makefile
toolkit/components/passwordmgr/base/Makefile
toolkit/components/passwordmgr/resources/Makefile
toolkit/components/passwordmgr/test/Makefile
toolkit/components/places/Makefile
toolkit/components/places/public/Makefile
toolkit/components/places/src/Makefile
toolkit/components/printing/Makefile
toolkit/components/satchel/Makefile
toolkit/components/satchel/public/Makefile
toolkit/components/satchel/src/Makefile
toolkit/components/startup/Makefile
toolkit/components/startup/public/Makefile
toolkit/components/startup/src/Makefile
toolkit/components/typeaheadfind/Makefile
toolkit/components/typeaheadfind/public/Makefile
toolkit/components/typeaheadfind/src/Makefile
toolkit/components/viewconfig/Makefile
toolkit/components/viewsource/Makefile
toolkit/locales/Makefile
toolkit/mozapps/Makefile
toolkit/mozapps/downloads/content/Makefile
toolkit/mozapps/downloads/Makefile
toolkit/mozapps/downloads/src/Makefile
toolkit/mozapps/extensions/Makefile
toolkit/mozapps/extensions/public/Makefile
toolkit/mozapps/extensions/src/Makefile
toolkit/mozapps/update/Makefile
toolkit/mozapps/update/public/Makefile
toolkit/mozapps/update/src/Makefile
toolkit/mozapps/xpinstall/Makefile
toolkit/profile/Makefile
toolkit/profile/public/Makefile
toolkit/profile/skin/Makefile
toolkit/profile/src/Makefile
toolkit/themes/Makefile
toolkit/themes/gnomestripe/global/Makefile
toolkit/themes/gnomestripe/Makefile
toolkit/themes/pmstripe/global/Makefile
toolkit/themes/pmstripe/Makefile
toolkit/themes/pinstripe/communicator/Makefile
toolkit/themes/pinstripe/Makefile
toolkit/themes/pinstripe/global/Makefile
toolkit/themes/pinstripe/help/Makefile
toolkit/themes/pinstripe/mozapps/Makefile
toolkit/themes/winstripe/communicator/Makefile
toolkit/themes/winstripe/Makefile
toolkit/themes/winstripe/global/Makefile
toolkit/themes/winstripe/help/Makefile
toolkit/themes/winstripe/mozapps/Makefile
toolkit/xre/Makefile
"


MAKEFILES_thunderbird="
mail/Makefile
mail/app/Makefile
mail/app/profile/Makefile
mail/base/Makefile
mail/locales/Makefile
mail/components/Makefile
mail/components/compose/Makefile
mail/components/addrbook/Makefile
mail/components/preferences/Makefile
mail/components/build/Makefile
mail/components/shell/Makefile
mail/components/shell/public/Makefile
mail/components/phishing/Makefile
mail/extensions/Makefile
mail/extensions/smime/Makefile
mail/config/Makefile
mail/installer/Makefile
mail/installer/windows/Makefile
mail/themes/Makefile
mail/themes/pinstripe/mail/Makefile
mail/themes/pinstripe/editor/Makefile
mail/themes/pinstripe/Makefile
mail/themes/qute/mail/Makefile
mail/themes/qute/editor/Makefile
mail/themes/qute/Makefile
"

MAKEFILES_standalone_composer="
composer/Makefile
composer/app/Makefile
composer/app/profile/Makefile
composer/base/Makefile
xpfe/components/build2/Makefile
"

MAKEFILES_calendar="
calendar/Makefile
calendar/resources/Makefile
calendar/libical/Makefile
calendar/libical/src/Makefile
calendar/libical/src/libical/Makefile
calendar/libical/src/libicalss/Makefile
calendar/base/Makefile
calendar/base/public/Makefile
calendar/base/src/Makefile
calendar/base/build/Makefile
calendar/providers/Makefile
calendar/providers/memory/Makefile
calendar/providers/storage/Makefile
calendar/providers/composite/Makefile
"

MAKEFILES_sunbird="
calendar/installer/Makefile
calendar/installer/windows/Makefile
calendar/locales/Makefile
calendar/sunbird/Makefile
calendar/sunbird/app/Makefile
calendar/sunbird/base/Makefile
"

MAKEFILES_macbrowser="
camino/Makefile
camino/flashblock/Makefile
camino/installer/Makefile
"

MAKEFILES_sql="
extensions/sql/Makefile
extensions/sql/base/Makefile
extensions/sql/base/public/Makefile
extensions/sql/base/src/Makefile
extensions/sql/base/resources/Makefile
extensions/sql/pgsql/public/Makefile
extensions/sql/pgsql/src/Makefile
extensions/sql/odbc/Makefile
extensions/sql/odbc/public/Makefile
extensions/sql/odbc/src/Makefile
extensions/sql/build/Makefile
extensions/sql/build/src/Makefile
extensions/sql/sqltest/Makefile
extensions/sql/tests/Makefile
"

if [ "$MOZ_MAIL_NEWS" ]; then
    if [ -f ${srcdir}/mailnews/makefiles ]; then
        MAKEFILES_mailnews=`cat ${srcdir}/mailnews/makefiles`
    fi
fi

MAKEFILES_ipcd="
ipc/ipcd/Makefile
ipc/ipcd/daemon/public/Makefile
ipc/ipcd/daemon/src/Makefile
ipc/ipcd/client/public/Makefile
ipc/ipcd/client/src/Makefile
ipc/ipcd/shared/src/Makefile
ipc/ipcd/test/Makefile
ipc/ipcd/test/module/Makefile
ipc/ipcd/extensions/Makefile
ipc/ipcd/extensions/lock/Makefile
ipc/ipcd/extensions/lock/public/Makefile
ipc/ipcd/extensions/lock/src/Makefile
ipc/ipcd/extensions/lock/src/module/Makefile
ipc/ipcd/util/Makefile
ipc/ipcd/util/public/Makefile
ipc/ipcd/util/src/Makefile
"

MAKEFILES_transmngr="
ipc/ipcd/extensions/transmngr/Makefile
ipc/ipcd/extensions/transmngr/public/Makefile
ipc/ipcd/extensions/transmngr/src/Makefile
ipc/ipcd/extensions/transmngr/build/Makefile
ipc/ipcd/extensions/transmngr/test/Makefile
ipc/ipcd/extensions/transmngr/common/Makefile
ipc/ipcd/extensions/transmngr/module/Makefile
"

MAKEFILES_profilesharingsetup="
embedding/components/profilesharingsetup/Makefile
embedding/components/profilesharingsetup/public/Makefile
embedding/components/profilesharingsetup/src/Makefile
"

    MAKEFILES_libpr0n="
        modules/libpr0n/Makefile
        modules/libpr0n/build/Makefile
        modules/libpr0n/public/Makefile
        modules/libpr0n/src/Makefile
        modules/libpr0n/decoders/Makefile
        modules/libpr0n/decoders/gif/Makefile
        modules/libpr0n/decoders/png/Makefile
        modules/libpr0n/decoders/jpeg/Makefile
        modules/libpr0n/decoders/bmp/Makefile
        modules/libpr0n/decoders/icon/Makefile
        modules/libpr0n/decoders/icon/win/Makefile
        modules/libpr0n/decoders/icon/gtk/Makefile
        modules/libpr0n/decoders/icon/beos/Makefile
        modules/libpr0n/decoders/xbm/Makefile
        modules/libpr0n/encoders/Makefile
        modules/libpr0n/encoders/png/Makefile
        modules/libpr0n/encoders/jpeg/Makefile
"

    MAKEFILES_accessible="
       accessible/Makefile
       accessible/public/Makefile
       accessible/public/msaa/Makefile
       accessible/src/Makefile
       accessible/src/base/Makefile
       accessible/src/html/Makefile
       accessible/src/xul/Makefile
       accessible/src/msaa/Makefile
       accessible/src/atk/Makefile
       accessible/src/mac/Makefile
       accessible/build/Makefile
"

if [ ! "$SYSTEM_JPEG" ]; then
    MAKEFILES_jpeg="jpeg/Makefile"
fi

if [ ! "$SYSTEM_ZLIB" ]; then
    MAKEFILES_zlib="
	modules/zlib/Makefile
	modules/zlib/src/Makefile
"
fi

MAKEFILES_zlib="
    $MAKEFILES_zlib
    modules/zlib/standalone/Makefile
"

MAKEFILES_libbz2="
    modules/libbz2/Makefile
    modules/libbz2/src/Makefile
"

MAKEFILES_libmar="
    modules/libmar/Makefile
    modules/libmar/src/Makefile
    modules/libmar/tool/Makefile
"

if test -n "$MOZ_UPDATE_PACKAGING"; then
    MAKEFILES_update_packaging="
        tools/update-packaging/Makefile
        other-licenses/bsdiff/Makefile
    "
fi

if [ ! "$SYSTEM_PNG" ]; then
    MAKEFILES_libimg="$MAKEFILES_libimg modules/libimg/png/Makefile"
fi

MAKEFILES_gnome="
    toolkit/Makefile
    toolkit/system/gnome/Makefile
"

#
# l10n/
#
if [ -f ${srcdir}/l10n/makefiles.all ]; then
    MAKEFILES_langpacks=`cat ${srcdir}/l10n/makefiles.all`
fi

if [ "$MOZ_L10N" ]; then
    MAKEFILES_l10n="l10n/Makefile"

    if [ "$MOZ_L10N_LANG" ]; then
	MAKEFILES_l10n_lang="
		l10n/lang/Makefile
		l10n/lang/addressbook/Makefile
		l10n/lang/bookmarks/Makefile
		l10n/lang/directory/Makefile
		l10n/lang/editor/Makefile
		l10n/lang/global/Makefile
		l10n/lang/history/Makefile
		l10n/lang/messenger/Makefile
		l10n/lang/messengercompose/Makefile
		l10n/lang/navigator/Makefile
		l10n/lang/pref/Makefile
		l10n/lang/related/Makefile
		l10n/lang/sidebar/Makefile
		l10n/lang/addressbook/locale/Makefile
		l10n/lang/bookmarks/locale/Makefile
		l10n/lang/directory/locale/Makefile
		l10n/lang/editor/locale/Makefile
		l10n/lang/global/locale/Makefile
		l10n/lang/history/locale/Makefile
		l10n/lang/messenger/locale/Makefile
		l10n/lang/messengercompose/locale/Makefile
		l10n/lang/navigator/locale/Makefile
		l10n/lang/pref/locale/Makefile
		l10n/lang/related/locale/Makefile
		l10n/lang/sidebar/locale/Makefile
"
	fi
fi

# tools/jprof
if [ "$MOZ_JPROF" ]; then
    MAKEFILES_jprof="tools/jprof/Makefile
        tools/jprof/stub/Makefile"
fi

# tools/leaky
if [ "$MOZ_LEAKY" ]; then
    MAKEFILES_leaky="tools/leaky/Makefile"
fi

# tools/trace-malloc
if [ "$NS_TRACE_MALLOC" ]; then
    MAKEFILES_tracemalloc="tools/trace-malloc/Makefile tools/trace-malloc/lib/Makefile"
fi

# tools/codesighs
if [ "$MOZ_MAPINFO" ]; then
    MAKEFILES_codesighs="tools/codesighs/Makefile"
fi

# MathML
if [ "$MOZ_MATHML" ]; then
    MAKEFILES_intl="$MAKEFILES_intl
	intl/uconv/ucvmath/Makefile
"
    MAKEFILES_layout="$MAKEFILES_layout
	layout/mathml/Makefile
	layout/mathml/base/Makefile
	layout/mathml/base/src/Makefile
	layout/mathml/content/Makefile
	layout/mathml/content/src/Makefile
"
fi

# svg
if [ "$MOZ_SVG" ]; then
    MAKEFILES_content="$MAKEFILES_content
	content/svg/Makefile
	content/svg/document/Makefile
	content/svg/document/src/Makefile
	content/svg/content/Makefile
	content/svg/content/src/Makefile
"
    MAKEFILES_dom="$MAKEFILES_dom
	dom/public/idl/svg/Makefile
"
    MAKEFILES_layout="$MAKEFILES_layout
	layout/svg/Makefile
	layout/svg/base/Makefile
	layout/svg/base/src/Makefile
"
fi

# xtf
if [ "$MOZ_XTF" ]; then
    MAKEFILES_content="$MAKEFILES_content
	content/xtf/Makefile
	content/xtf/public/Makefile
	content/xtf/src/Makefile
"
fi

if [ "$MOZ_XMLEXTRAS" ]; then
    MAKEFILES_content="$MAKEFILES_content
        extensions/xmlextras/Makefile
        extensions/xmlextras/pointers/Makefile
        extensions/xmlextras/pointers/src/Makefile
        extensions/xmlextras/build/Makefile
        extensions/xmlextras/build/src/Makefile
"
fi

if [ "$MOZ_WEBSERVICES" ]; then
    MAKEFILES_content="$MAKEFILES_content
        extensions/webservices/Makefile
        extensions/webservices/build/Makefile
        extensions/webservices/build/src/Makefile
        extensions/webservices/interfaceinfo/Makefile
        extensions/webservices/interfaceinfo/src/Makefile
        extensions/webservices/proxy/Makefile
        extensions/webservices/proxy/src/Makefile
        extensions/webservices/public/Makefile
        extensions/webservices/security/Makefile
        extensions/webservices/security/src/Makefile
        extensions/webservices/schema/Makefile
        extensions/webservices/schema/src/Makefile
        extensions/webservices/soap/Makefile
        extensions/webservices/soap/src/Makefile
        extensions/webservices/wsdl/Makefile
        extensions/webservices/wsdl/src/Makefile
"
fi

if [ "$MOZ_JAVAXPCOM" ]; then
    MAKEFILES_javaxpcom="
        extensions/java/Makefile
        extensions/java/xpcom/Makefile
        extensions/java/xpcom/interfaces/Makefile
        extensions/java/xpcom/src/Makefile
        extensions/java/xpcom/glue/Makefile
"
fi

# directory/xpcom
if [ "$MOZ_LDAP_XPCOM" ]; then
    MAKEFILES_ldap="
	directory/xpcom/Makefile
	directory/xpcom/base/Makefile
	directory/xpcom/base/public/Makefile
	directory/xpcom/base/src/Makefile
"
fi

# embedding/componentlib

if [ "$MOZ_COMPONENTLIB" ]; then
    MAKEFILES_static_components="$MAKEFILE_static_components
	embedding/componentlib/Makefile
"

else

# modules/staticmod

if [ "$MOZ_STATIC_COMPONENTS" -o "$MOZ_META_COMPONENTS" ]; then
    MAKEFILES_static_components="$MAKEFILES_static_components
	modules/staticmod/Makefile
"
fi
fi

if [ "$MOZ_PREF_EXTENSIONS" ]; then
    MAKEFILES_extensions="$MAKEFILES_extensions
        extensions/pref/Makefile
        extensions/pref/autoconfig/Makefile
        extensions/pref/autoconfig/public/Makefile
        extensions/pref/autoconfig/src/Makefile
        extensions/pref/autoconfig/resources/Makefile
"
fi

for extension in $MOZ_EXTENSIONS; do
    case "$extension" in
        access-builtin ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/access-builtin/Makefile
            extensions/access-builtin/accessproxy/Makefile
            " ;;
        cookie ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/cookie/Makefile
            " ;;
        cview ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/cview/Makefile
            extensions/cview/resources/Makefile
            " ;;
        datetime ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/datetime/Makefile
            " ;;
        finger ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/finger/Makefile
            " ;;
        gnomevfs ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/gnomevfs/Makefile
            " ;;
        help ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/help/Makefile
            extensions/help/resources/Makefile
            " ;;
        inspector ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_inspector"
            ;;
        spatialnavigation ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_spatialnavigation"
            ;;
        typeaheadfind ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_typeaheadfind"
            ;;
        irc ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/irc/Makefile
            " ;;
        layout-debug ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/layout-debug/Makefile
            extensions/layout-debug/src/Makefile
            extensions/layout-debug/ui/Makefile
            " ;;
        p3p ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/p3p/Makefile
            extensions/p3p/public/Makefile
            extensions/p3p/src/Makefile
            " ;;
        reporter ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/reporter/Makefile
	    extensions/reporter/locales/Makefile
            " ;;

        tasks ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/tasks/Makefile
            " ;;
        sroaming ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_sroaming"
            ;;
        transformiix ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_transformiix"
            ;;
        tridentprofile ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_tridentprofile"
            ;;
        venkman ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/venkman/Makefile
            extensions/venkman/resources/Makefile
            " ;;
        wallet ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/wallet/Makefile
            extensions/wallet/public/Makefile
            extensions/wallet/src/Makefile
            extensions/wallet/editor/Makefile
            extensions/wallet/signonviewer/Makefile
            extensions/wallet/walletpreview/Makefile
            extensions/wallet/build/Makefile
            " ;;
        xforms ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/xforms/Makefile
            " ;;
        xml-rpc ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/xml-rpc/Makefile
            extensions/xml-rpc/idl/Makefile
            extensions/xml-rpc/src/Makefile
            " ;;
        xmlterm ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/xmlterm/Makefile
            extensions/xmlterm/base/Makefile
            extensions/xmlterm/geckoterm/Makefile
            extensions/xmlterm/linetest/Makefile
            extensions/xmlterm/scripts/Makefile
            extensions/xmlterm/tests/Makefile
            extensions/xmlterm/ui/Makefile
            " ;;
        python ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/python/Makefile
            " ;;
        python/xpcom ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/python/xpcom/Makefile
            extensions/python/xpcom/components/Makefile
            extensions/python/xpcom/src/Makefile
            extensions/python/xpcom/src/loader/Makefile
            extensions/python/xpcom/src/module/Makefile
            extensions/python/xpcom/test/Makefile
            extensions/python/xpcom/test/test_component/Makefile
            " ;;
        python/dom ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/python/dom/Makefile
            extensions/python/dom/test/Makefile
            extensions/python/dom/test/pyxultest/Makefile
            extensions/python/dom/src/Makefile
            extensions/python/dom/nsdom/Makefile
            extensions/python/dom/nsdom/test/Makefile
            " ;;
        sql ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_sql"
            ;;
        schema-validation ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/schema-validation/Makefile
            extensions/schema-validation/public/Makefile
            extensions/schema-validation/src/Makefile
            " ;;
        permissions ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/permissions/Makefile
            " ;;
    esac
done

MAKEFILES_themes=`cat ${srcdir}/themes/makefiles`

add_makefiles "
$MAKEFILES_caps
$MAKEFILES_chrome
$MAKEFILES_db
$MAKEFILES_docshell
$MAKEFILES_dom
$MAKEFILES_editor
$MAKEFILES_codesighs
$MAKEFILES_composer
$MAKEFILES_embedding
$MAKEFILES_expat
$MAKEFILES_extensions
$MAKEFILES_gc
$MAKEFILES_gfx
$MAKEFILES_accessible
$MAKEFILES_htmlparser
$MAKEFILES_intl
$MAKEFILES_javaxpcom
$MAKEFILES_ldap
$MAKEFILES_leaky
$MAKEFILES_jpeg
$MAKEFILES_jprof
$MAKEFILES_js
$MAKEFILES_jsdebugger
$MAKEFILES_l10n
$MAKEFILES_l10n_lang
$MAKEFILES_langpacks
$MAKEFILES_content
$MAKEFILES_layout
$MAKEFILES_libart
$MAKEFILES_libreg
$MAKEFILES_libimg
$MAKEFILES_libpr0n
$MAKEFILES_libjar
$MAKEFILES_libpref
$MAKEFILES_libutil
$MAKEFILES_liveconnect
$MAKEFILES_macmorefiles
$MAKEFILES_mailnews
$MAKEFILES_oji
$MAKEFILES_plugin
$MAKEFILES_netwerk
$MAKEFILES_profile
$MAKEFILES_rdf
$MAKEFILES_static_components
$MAKEFILES_sun_java
$MAKEFILES_themes
$MAKEFILES_tracemalloc
$MAKEFILES_uriloader
$MAKEFILES_view
$MAKEFILES_webshell
$MAKEFILES_widget
$MAKEFILES_xft
$MAKEFILES_xpcom
$MAKEFILES_xpcom_obsolete
$MAKEFILES_xpcom_tests
$MAKEFILES_xpconnect
$MAKEFILES_xpinstall
$MAKEFILES_xpfe
$MAKEFILES_zlib
$MAKEFILES_libbz2
$MAKEFILES_libmar
$MAKEFILES_update_packaging
"

if test -n "$MOZ_PSM"; then
    add_makefiles "$MAKEFILES_psm2"
fi

if test -n "$MOZ_CALENDAR"; then
    add_makefiles "$MAKEFILES_calendar"
fi

if test -n "$MOZ_PHOENIX"; then
    add_makefiles "$MAKEFILES_phoenix"
fi

if test -n "$MOZ_SUITE"; then
    add_makefiles "$MAKEFILES_suite"
fi

if test -n "$MOZ_XUL_APP"; then
    add_makefiles "$MAKEFILES_xulapp"
fi

if test -n "$MOZ_XULRUNNER"; then
    add_makefiles "$MAKEFILES_xulrunner"
fi

if test -n "$MOZ_THUNDERBIRD"; then
    add_makefiles "$MAKEFILES_thunderbird"
fi

if test -n "$MOZ_STANDALONE_COMPOSER"; then
    add_makefiles "$MAKEFILES_standalone_composer"
fi

if test -n "$MOZ_SUNBIRD"; then
    add_makefiles "$MAKEFILES_sunbird"
fi

if test "$MOZ_BUILD_APP" = "camino"; then
    add_makefiles "$MAKEFILES_macbrowser"
fi

if test -n "$MOZ_IPCD"; then
    add_makefiles "$MAKEFILES_ipcd"
fi

if test -n "$MOZ_PROFILESHARING"; then
    add_makefiles "$MAKEFILES_transmngr"
    add_makefiles "$MAKEFILES_profilesharingsetup"
fi

if test -n "$MINIMO"; then
    add_makefiles "$MAKEFILES_minimo"
    add_makefiles "$MAKEFILES_xulapp"
fi

if test -n "$MOZ_ENABLE_GTK2"; then
    add_makefiles "$MAKEFILES_gnome"
fi

if test -n "$MOZ_STORAGE"; then
    add_makefiles "$MAKEFILES_storage"
fi
