#! /bin/sh
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

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
build/unix/mozilla-config
config/Makefile
config/autoconf.mk
config/mkdepend/Makefile
include/Makefile
"

MAKEFILES_db="
db/Makefile
db/mdb/Makefile
db/mdb/public/Makefile
db/mork/Makefile
db/mork/build/Makefile
db/mork/src/Makefile
"

MAKEFILES_dbm="
dbm/Makefile
dbm/include/Makefile
dbm/src/Makefile
dbm/tests/Makefile
"

MAKEFILES_dom="
dom/Makefile
dom/public/Makefile
dom/public/base/Makefile
dom/public/coreEvents/Makefile
dom/public/idl/Makefile
dom/public/idl/base/Makefile
dom/public/idl/core/Makefile
dom/public/idl/css/Makefile
dom/public/idl/events/Makefile
dom/public/idl/html/Makefile
dom/public/idl/range/Makefile
dom/public/idl/stylesheets/Makefile
dom/public/idl/views/Makefile
dom/public/idl/xbl/Makefile
dom/public/idl/xul/Makefile
dom/src/Makefile
dom/src/base/Makefile
dom/src/build/Makefile
dom/src/events/Makefile
dom/src/jsurl/Makefile
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
expat/Makefile
expat/xmlparse/Makefile
expat/xmltok/Makefile
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
gfx/src/photon/Makefile
gfx/src/mac/Makefile
gfx/src/qt/Makefile
gfx/src/xlib/Makefile
gfx/src/os2/Makefile
gfx/src/xlibrgb/Makefile
gfx/tests/Makefile
"

MAKEFILES_htmlparser="
htmlparser/Makefile
htmlparser/robot/Makefile
htmlparser/robot/test/Makefile
htmlparser/public/Makefile
htmlparser/src/Makefile
htmlparser/tests/Makefile
htmlparser/tests/grabpage/Makefile
htmlparser/tests/logparse/Makefile
htmlparser/tests/outsinks/Makefile
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
intl/locale/Makefile
intl/locale/public/Makefile
intl/locale/idl/Makefile
intl/locale/src/Makefile
intl/locale/src/unix/Makefile
intl/locale/src/os2/Makefile
intl/locale/tests/Makefile
intl/lwbrk/Makefile
intl/lwbrk/src/Makefile
intl/lwbrk/public/Makefile
intl/lwbrk/tests/Makefile
intl/unicharutil/Makefile
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

MAKEFILES_js="
js/Makefile
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
content/build/Makefile
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
content/html/style/Makefile
content/html/style/public/Makefile
content/html/style/src/Makefile
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
content/xbl/builtin/unix/Makefile
content/xbl/builtin/win/Makefile
content/xsl/Makefile
content/xsl/document/Makefile
content/xsl/document/src/Makefile
content/shared/Makefile
content/shared/public/Makefile
content/shared/src/Makefile
"

MAKEFILES_layout="
layout/Makefile
layout/base/Makefile
layout/base/public/Makefile
layout/base/src/Makefile
layout/base/tests/Makefile
layout/build/Makefile
layout/html/Makefile
layout/html/base/Makefile
layout/html/base/src/Makefile
layout/html/document/Makefile
layout/html/document/src/Makefile
layout/html/forms/Makefile
layout/html/forms/public/Makefile
layout/html/forms/src/Makefile
layout/html/style/Makefile
layout/html/style/src/Makefile
layout/html/table/Makefile
layout/html/table/public/Makefile
layout/html/table/src/Makefile
layout/html/tests/Makefile
layout/tools/Makefile
layout/xul/Makefile
layout/xul/base/Makefile
layout/xul/base/public/Makefile
layout/xul/base/src/Makefile
layout/xul/base/src/outliner/Makefile
layout/xul/base/src/outliner/src/Makefile
layout/xul/base/src/outliner/public/Makefile
"

MAKEFILES_mpfilelocprovider="
modules/mpfilelocprovider/Makefile
modules/mpfilelocprovider/public/Makefile
modules/mpfilelocprovider/src/Makefile
"

MAKEFILES_libimg="
modules/libimg/Makefile
"

MAKEFILES_libjar="
modules/libjar/Makefile
modules/libjar/standalone/Makefile
"

MAKEFILES_libreg="
modules/libreg/Makefile
modules/libreg/include/Makefile
modules/libreg/src/Makefile
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
"

MAKEFILES_plugin="
modules/plugin/Makefile
modules/plugin/base/src/Makefile
modules/plugin/base/public/Makefile
modules/plugin/samples/simple/Makefile
modules/plugin/samples/SanePlugin/Makefile
modules/plugin/samples/default/unix/Makefile
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
netwerk/build2/Makefile
netwerk/cache/Makefile
netwerk/cache/public/Makefile
netwerk/cache/src/Makefile
netwerk/dns/Makefile
netwerk/dns/public/Makefile
netwerk/dns/src/Makefile
netwerk/protocol/Makefile
netwerk/protocol/about/Makefile
netwerk/protocol/about/public/Makefile
netwerk/protocol/about/src/Makefile
netwerk/protocol/data/Makefile
netwerk/protocol/data/public/Makefile
netwerk/protocol/data/src/Makefile
netwerk/protocol/datetime/src/Makefile
netwerk/protocol/datetime/Makefile
netwerk/protocol/file/Makefile
netwerk/protocol/file/public/Makefile
netwerk/protocol/file/src/Makefile
netwerk/protocol/finger/Makefile
netwerk/protocol/finger/src/Makefile
netwerk/protocol/ftp/Makefile
netwerk/protocol/ftp/public/Makefile
netwerk/protocol/ftp/src/Makefile
netwerk/protocol/gopher/Makefile
netwerk/protocol/gopher/src/Makefile
netwerk/protocol/http/Makefile
netwerk/protocol/http/public/Makefile
netwerk/protocol/http/src/Makefile
netwerk/protocol/jar/Makefile
netwerk/protocol/jar/public/Makefile
netwerk/protocol/jar/src/Makefile
netwerk/protocol/keyword/Makefile
netwerk/protocol/keyword/src/Makefile
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

uriloader/exthandler/Makefile
intl/strres/public/Makefile
intl/locale/idl/Makefile
$MAKEFILES_js
modules/libpref/public/Makefile
"

MAKEFILES_uriloader="
uriloader/Makefile
uriloader/base/Makefile
uriloader/build/Makefile
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
"

MAKEFILES_rdf="
rdf/Makefile
rdf/base/Makefile
rdf/base/idl/Makefile
rdf/base/public/Makefile
rdf/base/src/Makefile
rdf/chrome/Makefile
rdf/chrome/build/Makefile
rdf/chrome/public/Makefile
rdf/chrome/src/Makefile
rdf/chrome/tools/Makefile
rdf/chrome/tools/chromereg/Makefile
rdf/util/Makefile
rdf/util/public/Makefile
rdf/util/src/Makefile
rdf/resources/Makefile
rdf/build/Makefile
rdf/datasource/Makefile
rdf/datasource/public/Makefile
rdf/datasource/src/Makefile
rdf/tests/Makefile
rdf/tests/localfile/Makefile
rdf/tests/rdfsink/Makefile
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

MAKEFILES_view="
view/Makefile
view/public/Makefile
view/src/Makefile
"

MAKEFILES_docshell="
docshell/Makefile
docshell/base/Makefile
docshell/build/Makefile
"

MAKEFILES_webshell="
webshell/Makefile
webshell/public/Makefile
webshell/src/Makefile
webshell/tests/Makefile
webshell/tests/viewer/Makefile
webshell/tests/viewer/public/Makefile
webshell/tests/viewer/unix/Makefile
webshell/tests/viewer/unix/gtk/Makefile
webshell/tests/viewer/unix/qt/Makefile
webshell/tests/viewer/unix/xlib/Makefile
webshell/embed/Makefile
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
widget/src/photon/Makefile
widget/src/mac/Makefile
widget/src/xlib/Makefile
widget/src/os2/Makefile
widget/src/os2/res/Makefile
widget/src/os2/tests/Makefile
widget/src/qt/Makefile
widget/src/xlib/window_service/Makefile
widget/src/xpwidgets/Makefile
widget/src/support/Makefile
widget/timer/Makefile
widget/timer/public/Makefile
widget/timer/src/Makefile
widget/timer/src/beos/Makefile
widget/timer/src/rhapsody/Makefile
widget/timer/src/unix/Makefile
widget/timer/src/unix/gtk/Makefile
widget/timer/src/unix/photon/Makefile
widget/timer/src/unix/xlib/Makefile
widget/timer/src/unix/qt/Makefile
widget/timer/src/os2/Makefile
"

MAKEFILES_xpcom="
string/Makefile
string/obsolete/Makefile
string/public/Makefile
string/src/Makefile
xpcom/Makefile
xpcom/base/Makefile
xpcom/build/Makefile
xpcom/components/Makefile
xpcom/ds/Makefile
xpcom/glue/Makefile
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

$MAKEFILES_libreg
$MAKEFILES_libjar
intl/unicharutil/public/Makefile
intl/uconv/public/Makefile
netwerk/base/public/Makefile
"

MAKEFILES_xpcom_tests="
xpcom/tests/Makefile
xpcom/tests/dynamic/Makefile
xpcom/tests/services/Makefile
"

MAKEFILES_string="$MAKEFILES_xpcom"

MAKEFILES_xpinstall="
xpinstall/Makefile
xpinstall/packager/Makefile
xpinstall/public/Makefile
xpinstall/res/Makefile
xpinstall/src/Makefile
xpinstall/stub/Makefile
xpinstall/wizard/libxpnet/Makefile
xpinstall/wizard/libxpnet/src/Makefile
xpinstall/wizard/libxpnet/test/Makefile
xpinstall/wizard/unix/src2/Makefile
"

MAKEFILES_xpfe="
xpfe/Makefile
xpfe/browser/Makefile
xpfe/browser/public/Makefile
xpfe/browser/src/Makefile
xpfe/browser/samples/Makefile
xpfe/browser/samples/sampleimages/Makefile
xpfe/components/Makefile
xpfe/components/shistory/Makefile
xpfe/components/shistory/public/Makefile
xpfe/components/shistory/src/Makefile
xpfe/components/bookmarks/Makefile
xpfe/components/bookmarks/public/Makefile
xpfe/components/bookmarks/src/Makefile
xpfe/components/directory/Makefile
xpfe/components/timebomb/Makefile
xpfe/components/timebomb/tools/Makefile
xpfe/components/regviewer/Makefile
xpfe/components/find/Makefile
xpfe/components/find/public/Makefile
xpfe/components/find/src/Makefile
xpfe/components/filepicker/Makefile
xpfe/components/filepicker/public/Makefile
xpfe/components/filepicker/src/Makefile
xpfe/components/history/Makefile
xpfe/components/history/src/Makefile
xpfe/components/history/public/Makefile
xpfe/components/prefwindow/Makefile
xpfe/components/prefwindow/resources/Makefile
xpfe/components/prefwindow/resources/content/Makefile
xpfe/components/prefwindow/resources/content/unix/Makefile
xpfe/components/prefwindow/resources/locale/Makefile
xpfe/components/prefwindow/resources/locale/en-US/Makefile
xpfe/components/prefwindow/resources/locale/en-US/unix/Makefile
xpfe/components/prefwindow/resources/locale/en-US/win/Makefile
xpfe/components/related/Makefile
xpfe/components/related/src/Makefile
xpfe/components/related/public/Makefile
xpfe/components/search/Makefile
xpfe/components/search/datasets/Makefile
xpfe/components/search/public/Makefile
xpfe/components/search/src/Makefile
xpfe/components/sidebar/Makefile
xpfe/components/sidebar/public/Makefile
xpfe/components/sidebar/src/Makefile
xpfe/components/xfer/Makefile
xpfe/components/xfer/public/Makefile
xpfe/components/xfer/src/Makefile
xpfe/components/ucth/Makefile
xpfe/components/ucth/public/Makefile
xpfe/components/ucth/src/Makefile
xpfe/components/autocomplete/Makefile
xpfe/components/autocomplete/public/Makefile
xpfe/components/autocomplete/src/Makefile
xpfe/components/urlbarhistory/Makefile
xpfe/components/urlbarhistory/public/Makefile
xpfe/components/urlbarhistory/src/Makefile
xpfe/components/console/Makefile
xpfe/components/build/Makefile
xpfe/appshell/Makefile
xpfe/appshell/src/Makefile
xpfe/appshell/public/Makefile
xpfe/bootstrap/Makefile
xpfe/browser/Makefile
xpfe/browser/src/Makefile
xpfe/browser/resources/Makefile
xpfe/browser/resources/content/Makefile
xpfe/browser/resources/content/unix/Makefile
xpfe/browser/resources/content/win/Makefile
xpfe/browser/resources/locale/Makefile
xpfe/browser/resources/locale/en-US/Makefile
xpfe/browser/resources/locale/en-US/unix/Makefile
xpfe/global/Makefile
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
xpfe/communicator/resources/Makefile
xpfe/communicator/resources/locale/Makefile
xpfe/communicator/resources/locale/en-US/Makefile
xpfe/communicator/resources/content/Makefile
xpfe/communicator/resources/content/unix/Makefile
xpfe/communicator/resources/content/win/Makefile
"

MAKEFILES_embedding="
embedding/Makefile
embedding/base/Makefile
embedding/browser/Makefile
embedding/browser/build/Makefile
embedding/browser/chrome/Makefile
embedding/browser/webBrowser/Makefile
embedding/browser/gtk/Makefile
embedding/browser/gtk/src/Makefile
embedding/browser/gtk/tests/Makefile
embedding/browser/photon/Makefile
embedding/browser/photon/src/Makefile
embedding/browser/photon/tests/Makefile
embedding/components/Makefile
embedding/components/build/Makefile
embedding/components/windowwatcher/Makefile
embedding/components/windowwatcher/public/Makefile
embedding/components/windowwatcher/src/Makefile
embedding/components/ui/Makefile
embedding/components/ui/helperAppDlg/Makefile
embedding/config/Makefile
"

MAKEFILES_psm2="
security/manager/Makefile
security/manager/ssl/Makefile
security/manager/ssl/src/Makefile
security/manager/ssl/resources/Makefile
security/manager/ssl/public/Makefile
security/manager/pki/Makefile
security/manager/pki/resources/Makefile
security/manager/pki/src/Makefile
security/manager/pki/public/Makefile
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
layout/html/forms/public/Makefile
gfx/public/Makefile
gfx/idl/Makefile
widget/public/Makefile
layout/base/public/Makefile
docshell/base/Makefile
modules/libpref/public/Makefile
content/base/public/Makefile
intl/locale/public/Makefile
"

MAKEFILES_inspector="
extensions/inspector/Makefile
extensions/inspector/base/Makefile
extensions/inspector/base/public/Makefile
extensions/inspector/base/src/Makefile
extensions/inspector/build/Makefile
extensions/inspector/build/src/Makefile
extensions/inspector/resources/Makefile
"

MAKEFILES_transformiix="
extensions/transformiix/source/base/Makefile
extensions/transformiix/source/main/Makefile
extensions/transformiix/source/net/Makefile
extensions/transformiix/source/xml/dom/standalone/Makefile
extensions/transformiix/source/xml/dom/Makefile
extensions/transformiix/source/xml/dom/mozImpl/Makefile
extensions/transformiix/source/xml/parser/Makefile
extensions/transformiix/source/xml/printer/Makefile
extensions/transformiix/source/xml/Makefile
extensions/transformiix/source/xpath/Makefile
extensions/transformiix/source/xslt/functions/Makefile
extensions/transformiix/source/xslt/util/Makefile
extensions/transformiix/source/xslt/Makefile
extensions/transformiix/source/Makefile
extensions/transformiix/Makefile
"

if [ "$MACOSX" ]; then
    MAKEFILES_macmorefiles="
       lib/mac/MoreFiles/Makefile
"
fi

if [ "$MOZ_MAIL_NEWS" ]; then
    if [ -f ${srcdir}/mailnews/makefiles ]; then
        MAKEFILES_mailnews=`cat ${srcdir}/mailnews/makefiles`
    fi
fi


    MAKEFILES_libpr0n="
        modules/libpr0n/Makefile
        modules/libpr0n/public/Makefile
        modules/libpr0n/src/Makefile
        modules/libpr0n/decoders/Makefile
        modules/libpr0n/decoders/gif/Makefile
        modules/libpr0n/decoders/png/Makefile
        modules/libpr0n/decoders/ppm/Makefile
        modules/libpr0n/decoders/jpeg/Makefile
        modules/libpr0n/decoders/bmp/Makefile
"

    MAKEFILES_gfx2="
        gfx2/Makefile
        gfx2/public/Makefile
        gfx2/src/Makefile
"
 
    MAKEFILES_accessible="
       accessible/Makefile
       accessible/public/Makefile
       accessible/src/Makefile
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


if [ ! "$SYSTEM_PNG" ]; then
    MAKEFILES_libimg="$MAKEFILES_libimg modules/libimg/png/Makefile"
fi

if [ ! "$SYSTEM_MNG" ]; then
    MAKEFILES_libimg="$MAKEFILES_libimg modules/libimg/mng/Makefile"
fi

#
#  java/
#
if [ "$MOZ_JAVA_SUPPLEMENT" ]; then
    MAKEFILES_java_supplement=`cat ${srcdir}/java/makefiles`
fi

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
    MAKEFILES_tracemalloc="tools/trace-malloc/Makefile"
fi

# layout/mathml
if [ "$MOZ_MATHML" ]; then
    MAKEFILES_layout="$MAKEFILES_layout
	layout/mathml/Makefile
	layout/mathml/base/Makefile
	layout/mathml/base/src/Makefile
	layout/mathml/content/Makefile
	layout/mathml/content/src/Makefile
"
fi

# layout/svg
if [ "$MOZ_SVG" ]; then
    MAKEFILES_layout="$MAKEFILES_layout
	layout/svg/Makefile
	layout/svg/base/Makefile
	layout/svg/base/public/Makefile
	layout/svg/base/src/Makefile
	layout/svg/content/Makefile
	layout/svg/content/src/Makefile
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

# modules/staticmod

if [ "$MOZ_STATIC_COMPONENTS" ]; then
    MAKEFILES_static_components="$MAKEFILES_static_components
	modules/staticmod/Makefile
"
fi

for extension in $MOZ_EXTENSIONS; do
    case "$extension" in
        access-builtin ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/access-builtin/Makefile
            extensions/access-builtin/accessproxy/Makefile
            " ;;
        content-packs ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/content-packs/Makefile
            extensions/content-packs/resources/Makefile
            " ;;
        cookie ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/cookie/Makefile
            extensions/cookie/tests/Makefile
            " ;;
        ctl ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/ctl/Makefile
            extensions/ctl/public/Makefile
            extensions/ctl/src/Makefile
            extensions/ctl/src/pangoLite/Makefile
            extensions/ctl/src/thaiShaper/Makefile
            " ;;
        cview ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/cview/Makefile
            extensions/cview/resources/Makefile
            " ;;
        help ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/help/Makefile
            extensions/help/resources/Makefile
            " ;;
        inspector ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_inspector"
            ;;
        irc ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/irc/Makefile
            " ;;
        p3p ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/p3p/Makefile
            extensions/p3p/public/Makefile
            extensions/p3p/resources/Makefile
            extensions/p3p/resources/content/Makefile
            extensions/p3p/resources/locale/Makefile
            extensions/p3p/resources/locale/en-US/Makefile
            extensions/p3p/resources/skin/Makefile
            extensions/p3p/src/Makefile
            " ;;
        pics ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/pics/Makefile
            extensions/pics/public/Makefile
            extensions/pics/src/Makefile
            extensions/pics/tests/Makefile
            " ;;
        transformiix ) MAKEFILES_extensions="$MAKEFILES_extensions
            $MAKEFILES_transformiix"
            ;;
        universalchardet ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/universalchardet/Makefile
            extensions/universalchardet/src/Makefile
            extensions/universalchardet/tests/Makefile
            " ;;
        venkman ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/venkman/Makefile"
            ;;
        wallet ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/wallet/Makefile
            extensions/wallet/public/Makefile
            extensions/wallet/src/Makefile
            extensions/wallet/editor/Makefile
            extensions/wallet/cookieviewer/Makefile
            extensions/wallet/signonviewer/Makefile
            extensions/wallet/walletpreview/Makefile
            extensions/wallet/build/Makefile
            " ;;
        xmlextras ) MAKEFILES_extensions="$MAKEFILES_extensions
            extensions/xmlextras/Makefile
            extensions/xmlextras/base/Makefile
            extensions/xmlextras/base/src/Makefile
            extensions/xmlextras/base/public/Makefile
            extensions/xmlextras/build/Makefile
            extensions/xmlextras/build/src/Makefile
            extensions/xmlextras/schema/Makefile
            extensions/xmlextras/schema/public/Makefile
            extensions/xmlextras/schema/src/Makefile
            extensions/xmlextras/soap/Makefile
            extensions/xmlextras/soap/public/Makefile
            extensions/xmlextras/soap/src/Makefile
            extensions/xmlextras/tests/Makefile
            extensions/xmlextras/wsdl/Makefile
            extensions/xmlextras/wsdl/public/Makefile
            extensions/xmlextras/wsdl/src/Makefile
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
    esac
done


#
# Translate from BUILD_MODULES into the proper makefiles list
#
if [ "$BUILD_MODULES" = "all" ]; then

MAKEFILES_themes=`cat ${srcdir}/themes/makefiles`

add_makefiles "
$MAKEFILES_caps
$MAKEFILES_db
$MAKEFILES_dbm
$MAKEFILES_docshell
$MAKEFILES_dom
$MAKEFILES_editor
$MAKEFILES_embedding
$MAKEFILES_expat
$MAKEFILES_extensions
$MAKEFILES_gc
$MAKEFILES_gfx
$MAKEFILES_gfx2
$MAKEFILES_accessible
$MAKEFILES_htmlparser
$MAKEFILES_intl
$MAKEFILES_java_supplement
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
$MAKEFILES_libreg
$MAKEFILES_libimg
$MAKEFILES_libpr0n
$MAKEFILES_libjar
$MAKEFILES_libpref
$MAKEFILES_libutil
$MAKEFILES_liveconnect
$MAKEFILES_mailnews
$MAKEFILES_mpfilelocprovider
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
$MAKEFILES_xpcom
$MAKEFILES_xpcom_tests
$MAKEFILES_xpconnect
$MAKEFILES_xpinstall
$MAKEFILES_xpfe
$MAKEFILES_zlib
"

if test -n "$MOZ_PSM"; then
    add_makefiles "$MAKEFILES_psm2"
fi

else

# Standalone modules go here
    for mod in $BUILD_MODULES; do
	case $mod in
	    dbm) add_makefiles "$MAKEFILES_dbm"
		;;
	    js) add_makefiles "$MAKEFILES_js"
		;;
	    necko) add_makefiles "
                 $MAKEFILES_netwerk $MAKEFILES_dbm $MAKEFILES_xpcom"
		;;
	    psm2) add_makefiles "$MAKEFILES_dbm $MAKEFILES_js $MAKEFILES_xpcom $MAKEFILES_psm2"
		;;
            string) add_makefiles "$MAKEFILES_string"
                ;;
	    transformiix) add_makefiles "$MAKEFILES_transformiix"
		;;
	    access-builtin) add_makefiles "$MAKEFILES_access_builtin"
		;;
	    xpcom) add_makefiles "$MAKEFILES_xpcom"

		;;
	    xpconnect) add_makefiles "
		$MAKEFILES_xpconnect $MAKEFILES_js $MAKEFILES_xpcom"
		;;
        esac
    done
fi
