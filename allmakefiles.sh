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
build/unix/nspr_my_config.mk
build/unix/nspr_my_overrides.mk
config/Makefile
config/autoconf.mk
config/mkdepend/Makefile
config/mkdetect/Makefile
include/Makefile
tools/elf-dynstr-gc/Makefile
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
dom/public/coreDom/Makefile
dom/public/coreEvents/Makefile
dom/public/css/Makefile
dom/public/events/Makefile
dom/public/range/Makefile
dom/public/html/Makefile
dom/public/idl/Makefile
dom/public/idl/base/Makefile
dom/public/idl/coreDom/Makefile
dom/public/idl/css/Makefile
dom/public/idl/events/Makefile
dom/public/idl/html/Makefile
dom/public/idl/range/Makefile
dom/src/Makefile
dom/src/base/Makefile
dom/src/build/Makefile
dom/src/coreDOM/Makefile
dom/src/css/Makefile
dom/src/events/Makefile
dom/src/range/Makefile
dom/src/html/Makefile
dom/src/jsurl/Makefile
dom/tools/Makefile
"

MAKEFILES_editor="
editor/Makefile
editor/base/Makefile
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
editor/ui/Makefile
editor/ui/composer/Makefile
editor/ui/composer/content/Makefile
editor/ui/composer/content/images/Makefile
editor/ui/composer/locale/Makefile
editor/ui/composer/locale/en-US/Makefile
editor/ui/dialogs/Makefile
editor/ui/dialogs/content/Makefile
editor/ui/dialogs/locale/Makefile
editor/ui/dialogs/locale/en-US/Makefile
"

MAKEFILES_expat="
expat/Makefile
expat/xmlparse/Makefile
expat/xmltok/Makefile
"

MAKEFILES_extensions="
extensions/Makefile
"

MAKEFILES_gfx="
gfx/Makefile
gfx/idl/Makefile
gfx/public/Makefile
gfx/src/Makefile
gfx/src/beos/Makefile
gfx/src/gtk/Makefile
gfx/src/ps/Makefile
gfx/src/motif/Makefile
gfx/src/photon/Makefile
gfx/src/rhapsody/Makefile
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

MAKEFILES_layout="
layout/Makefile
layout/base/Makefile
layout/base/public/Makefile
layout/base/src/Makefile
layout/base/tests/Makefile
layout/build/Makefile
layout/events/Makefile
layout/events/public/Makefile
layout/events/src/Makefile
layout/html/Makefile
layout/html/base/Makefile
layout/html/base/src/Makefile
layout/html/content/Makefile
layout/html/content/public/Makefile
layout/html/content/src/Makefile
layout/html/document/Makefile
layout/html/document/public/Makefile
layout/html/document/src/Makefile
layout/html/forms/Makefile
layout/html/forms/public/Makefile
layout/html/forms/src/Makefile
layout/html/style/Makefile
layout/html/style/public/Makefile
layout/html/style/src/Makefile
layout/html/table/Makefile
layout/html/table/public/Makefile
layout/html/table/src/Makefile
layout/html/tests/Makefile
layout/tools/Makefile
layout/xml/Makefile
layout/xml/content/Makefile
layout/xml/content/public/Makefile
layout/xml/content/src/Makefile
layout/xml/document/Makefile
layout/xml/document/public/Makefile
layout/xml/document/src/Makefile
layout/xul/Makefile
layout/xul/base/Makefile
layout/xul/base/public/Makefile
layout/xul/base/src/Makefile
layout/xul/content/Makefile
layout/xul/content/src/Makefile
layout/xbl/Makefile
layout/xbl/public/Makefile
layout/xbl/src/Makefile
"

MAKEFILES_libimg="
modules/libimg/Makefile
modules/libimg/public/Makefile
modules/libimg/public_com/Makefile
modules/libimg/src/Makefile
modules/libimg/gifcom/Makefile
modules/libimg/jpgcom/Makefile
modules/libimg/pngcom/Makefile  
"

MAKEFILES_libjar="
modules/libjar/Makefile
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
modules/plugin/nglsrc/Makefile
modules/plugin/public/Makefile
modules/plugin/src/Makefile
modules/plugin/test/Makefile
modules/plugin/SanePlugin/Makefile
"

MAKEFILES_netwerk="
netwerk/Makefile
netwerk/base/Makefile
netwerk/base/public/Makefile
netwerk/base/src/Makefile
netwerk/build/Makefile
netwerk/cache/build/Makefile
netwerk/cache/filecache/Makefile
netwerk/cache/Makefile
netwerk/cache/memcache/Makefile
netwerk/cache/mgr/Makefile
netwerk/cache/public/Makefile
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
"

MAKEFILES_uriloader="
uriloader/Makefile
uriloader/base/Makefile
uriloader/build/Makefile
uriloader/extprotocol/Makefile
uriloader/extprotocol/base/Makefile
uriloader/extprotocol/unix/Makefile
uriloader/exthandler/Makefile
"

MAKEFILES_profile="
profile/Makefile
profile/src/Makefile
profile/public/Makefile
profile/resources/Makefile
profile/resources/content/Makefile
profile/resources/locale/Makefile
profile/resources/locale/en-US/Makefile
profile/pref-migrator/Makefile
profile/pref-migrator/public/Makefile
profile/pref-migrator/src/Makefile
profile/pref-migrator/resources/Makefile
profile/pref-migrator/resources/content/Makefile
profile/pref-migrator/resources/locale/Makefile
profile/pref-migrator/resources/locale/en-US/Makefile
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
rdf/util/Makefile
rdf/util/public/Makefile
rdf/util/src/Makefile
rdf/resources/Makefile
rdf/build/Makefile
rdf/content/Makefile
rdf/content/public/Makefile
rdf/content/public/idl/Makefile
rdf/content/src/Makefile
rdf/datasource/Makefile
rdf/datasource/public/Makefile
rdf/datasource/src/Makefile
rdf/tests/Makefile
rdf/tests/domds/Makefile
rdf/tests/domds/resources/Makefile
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
sun-java/stubs/src/Makefile
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
webshell/tests/viewer/unix/motif/Makefile
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
widget/src/motif/Makefile
widget/src/motif/app_context/Makefile
widget/src/photon/Makefile
widget/src/rhapsody/Makefile
widget/src/mac/Makefile
widget/src/xlib/Makefile
widget/src/os2/Makefile
widget/src/os2/res/Makefile
widget/src/os2/tests/Makefile
widget/src/qt/Makefile
widget/src/xlib/window_service/Makefile
widget/src/xpwidgets/Makefile
widget/src/support/Makefile
widget/tests/Makefile
widget/tests/scribble/Makefile
widget/tests/widget/Makefile
widget/timer/Makefile
widget/timer/public/Makefile
widget/timer/src/Makefile
widget/timer/src/beos/Makefile
widget/timer/src/rhapsody/Makefile
widget/timer/src/unix/Makefile
widget/timer/src/unix/gtk/Makefile
widget/timer/src/unix/motif/Makefile
widget/timer/src/unix/photon/Makefile
widget/timer/src/unix/xlib/Makefile
widget/timer/src/unix/qt/Makefile
"

MAKEFILES_xpcom="
xpcom/Makefile
xpcom/base/Makefile
xpcom/build/Makefile
xpcom/components/Makefile
xpcom/ds/Makefile
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
xpcom/tests/Makefile
xpcom/tests/dynamic/Makefile
xpcom/tests/services/Makefile
xpcom/threads/Makefile
xpcom/tools/Makefile
xpcom/tools/registry/Makefile
"

MAKEFILES_xpinstall="
xpinstall/Makefile
xpinstall/packager/Makefile
xpinstall/public/Makefile
xpinstall/res/Makefile
xpinstall/res/content/Makefile
xpinstall/res/locale/Makefile
xpinstall/res/locale/en-US/Makefile
xpinstall/src/Makefile
xpinstall/stub/Makefile
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
xpfe/components/public/Makefile
xpfe/components/sample/Makefile
xpfe/components/sample/public/Makefile
xpfe/components/sample/src/Makefile
xpfe/components/sample/resources/Makefile
xpfe/components/shistory/Makefile
xpfe/components/shistory/public/Makefile
xpfe/components/shistory/src/Makefile
xpfe/components/bookmarks/Makefile
xpfe/components/bookmarks/public/Makefile
xpfe/components/bookmarks/src/Makefile
xpfe/components/bookmarks/resources/Makefile
xpfe/components/directory/Makefile
xpfe/components/timebomb/Makefile
xpfe/components/timebomb/tools/Makefile
xpfe/components/timebomb/resources/Makefile
xpfe/components/timebomb/resources/content/Makefile
xpfe/components/timebomb/resources/locale/Makefile
xpfe/components/timebomb/resources/locale/en-US/Makefile
xpfe/components/regviewer/Makefile
xpfe/components/find/Makefile
xpfe/components/find/public/Makefile
xpfe/components/find/src/Makefile
xpfe/components/find/resources/Makefile
xpfe/components/filepicker/src/Makefile
xpfe/components/filepicker/res/content/Makefile
xpfe/components/filepicker/res/locale/Makefile
xpfe/components/filepicker/res/locale/en-US/Makefile
xpfe/components/filepicker/res/Makefile
xpfe/components/filepicker/Makefile
xpfe/components/history/Makefile
xpfe/components/history/src/Makefile
xpfe/components/history/public/Makefile
xpfe/components/history/resources/Makefile
xpfe/components/prefwindow/Makefile
xpfe/components/prefwindow/resources/Makefile
xpfe/components/prefwindow/resources/content/Makefile
xpfe/components/prefwindow/resources/content/unix/Makefile
xpfe/components/prefwindow/resources/locale/Makefile
xpfe/components/prefwindow/resources/locale/en-US/Makefile
xpfe/components/prefwindow/resources/locale/en-US/unix/Makefile
xpfe/components/related/Makefile
xpfe/components/related/src/Makefile
xpfe/components/related/public/Makefile
xpfe/components/related/resources/Makefile
xpfe/components/search/Makefile
xpfe/components/search/datasets/Makefile
xpfe/components/search/resources/Makefile
xpfe/components/search/public/Makefile
xpfe/components/search/src/Makefile
xpfe/components/sidebar/Makefile
xpfe/components/sidebar/public/Makefile
xpfe/components/sidebar/resources/Makefile
xpfe/components/sidebar/src/Makefile
xpfe/components/xfer/Makefile
xpfe/components/xfer/public/Makefile
xpfe/components/xfer/src/Makefile
xpfe/components/xfer/resources/Makefile
xpfe/components/ucth/Makefile
xpfe/components/ucth/public/Makefile
xpfe/components/ucth/src/Makefile
xpfe/components/ucth/resources/Makefile
xpfe/components/remote/Makefile
xpfe/components/remote/public/Makefile
xpfe/components/remote/src/Makefile
xpfe/components/autocomplete/Makefile
xpfe/components/autocomplete/public/Makefile
xpfe/components/autocomplete/resources/Makefile
xpfe/components/autocomplete/resources/content/Makefile
xpfe/components/autocomplete/src/Makefile
xpfe/components/console/Makefile
xpfe/components/console/resources/Makefile
xpfe/components/console/resources/content/Makefile
xpfe/components/console/resources/locale/Makefile
xpfe/components/console/resources/locale/en-US/Makefile
xpfe/appshell/Makefile
xpfe/appshell/src/Makefile
xpfe/appshell/public/Makefile
xpfe/bootstrap/Makefile
xpfe/browser/Makefile
xpfe/browser/src/Makefile
xpfe/browser/resources/Makefile
xpfe/browser/resources/content/Makefile
xpfe/browser/resources/content/unix/Makefile
xpfe/browser/resources/locale/Makefile
xpfe/browser/resources/locale/en-US/Makefile
xpfe/browser/resources/locale/en-US/unix/Makefile
xpfe/appfilelocprovider/Makefile
xpfe/appfilelocprovider/public/Makefile
xpfe/appfilelocprovider/src/Makefile
xpfe/global/Makefile
xpfe/global/resources/Makefile
xpfe/global/resources/content/Makefile
xpfe/global/resources/content/os2/Makefile
xpfe/global/resources/content/unix/Makefile
xpfe/global/resources/locale/Makefile
xpfe/global/resources/locale/en-US/Makefile
xpfe/global/resources/locale/en-US/os2/Makefile
xpfe/global/resources/locale/en-US/unix/Makefile
xpfe/communicator/Makefile
xpfe/communicator/resources/Makefile
xpfe/communicator/resources/locale/Makefile
xpfe/communicator/resources/locale/en-US/Makefile
xpfe/communicator/resources/content/Makefile
"

MAKEFILES_embedding="
embedding/Makefile
embedding/browser/Makefile
embedding/browser/build/Makefile
embedding/browser/webBrowser/Makefile
embedding/browser/gtk/Makefile
embedding/browser/gtk/src/Makefile
embedding/browser/gtk/tests/Makefile
embedding/browser/photon/Makefile
embedding/browser/photon/src/Makefile
embedding/browser/photon/tests/Makefile
embedding/config/Makefile
embedding/tests/gtkEmbed/Makefile
"

MAKEFILES_security="
security/Makefile
security/base/Makefile
security/base/public/Makefile
security/base/res/Makefile
security/base/res/content/Makefile
security/base/res/locale/Makefile
security/base/res/locale/en-US/Makefile
security/psm/Makefile
security/psm/lib/Makefile
security/psm/lib/client/Makefile
security/psm/lib/protocol/Makefile
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
extensions/transformiix/source/xml/util/Makefile
extensions/transformiix/source/xml/Makefile
extensions/transformiix/source/xpath/Makefile
extensions/transformiix/source/xslt/functions/Makefile
extensions/transformiix/source/xslt/util/Makefile
extensions/transformiix/source/xslt/Makefile
extensions/transformiix/source/Makefile
extensions/transformiix/Makefile
"

if [ "$MOZ_MAIL_NEWS" ]; then
    MAKEFILES_mailnews=`cat ${srcdir}/mailnews/makefiles`
fi

if [ ! "$SYSTEM_JPEG" ]; then
    MAKEFILES_jpeg="jpeg/Makefile"
fi

if [ ! "$SYSTEM_ZLIB" ]; then
    MAKEFILES_zlib="
	modules/zlib/Makefile
	modules/zlib/src/Makefile
"
fi

if [ ! "$SYSTEM_PNG" ]; then
    MAKEFILES_libimg="$MAKEFILES_libimg modules/libimg/png/Makefile"
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
MAKEFILES_langpacks=`cat ${srcdir}/l10n/makefiles.all`

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
    MAKEFILES_jprof="tools/jprof/Makefile"
fi

# tools/leaky
if [ "$MOZ_LEAKY" ]; then
    MAKEFILES_leaky="tools/leaky/Makefile"
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

# libimg/mng
if [ "$MOZ_MNG" ]; then
    MAKEFILES_libimg="$MAKEFILES_libimg
	modules/libimg/mng/Makefile
	modules/libimg/mngcom/Makefile
"
fi

for extension in $MOZ_EXTENSIONS; do
    case "$extension" in
        cookie ) MAKEFILES_extensions="$MAKEFILES_extensions
	    extensions/cookie/Makefile
	    extensions/cookie/tests/Makefile
            " ;;
        psm-glue ) MAKEFILES_extensions="$MAKEFILES_extensions
	    extensions/psm-glue/public/Makefile
	    extensions/psm-glue/Makefile
	    extensions/psm-glue/src/Makefile
            " ;;
        irc ) MAKEFILES_extensions="$MAKEFILES_extensions
	    extensions/irc/Makefile
	    extensions/irc/xul/Makefile
	    extensions/irc/xul/content/Makefile
	    extensions/irc/xul/locale/Makefile
	    extensions/irc/xul/locale/en-US/Makefile
            " ;;
	transformiix ) MAKEFILES_extensions="$MAKEFILES_extensions
	    $MAKEFILES_transformiix"
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
	    extensions/xmlextras/soap/public/Makefile
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
        xml-rpc ) MAKEFILES_extensions="$MAKEFILES_extensions
	    extensions/xml-rpc/Makefile
	    extensions/xml-rpc/idl/Makefile
	    extensions/xml-rpc/src/Makefile
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
$MAKEFILES_gfx
$MAKEFILES_htmlparser
$MAKEFILES_intl
$MAKEFILES_java_supplement
$MAKEFILES_ldap
$MAKEFILES_leaky
$MAKEFILES_jpeg
$MAKEFILES_jprof
$MAKEFILES_js
$MAKEFILES_l10n
$MAKEFILES_l10n_lang
$MAKEFILES_langpacks
$MAKEFILES_layout
$MAKEFILES_libreg
$MAKEFILES_libimg
$MAKEFILES_libjar
$MAKEFILES_libpref
$MAKEFILES_libutil
$MAKEFILES_liveconnect
$MAKEFILES_mailnews
$MAKEFILES_oji
$MAKEFILES_plugin
$MAKEFILES_netwerk
$MAKEFILES_profile
$MAKEFILES_rdf
$MAKEFILES_security
$MAKEFILES_sun_java
$MAKEFILES_themes
$MAKEFILES_uriloader
$MAKEFILES_view
$MAKEFILES_webshell
$MAKEFILES_widget
$MAKEFILES_xpcom
$MAKEFILES_xpconnect
$MAKEFILES_xpinstall
$MAKEFILES_xpfe
$MAKEFILES_zlib
"

else

# Standalone modules go here
    for mod in $BUILD_MODULES; do
	case $mod in
	    dbm) add_makefiles "$MAKEFILES_dbm"
		;;
	    js) add_makefiles "$MAKEFILES_js"
		;;
	    necko) add_makefiles "
                 $MAKEFILES_netwerk $MAKEFILES_xpcom $MAKEFILES_libreg"
		;;
	    transformiix) add_makefiles "$MAKEFILES_transformiix"
		;;
	    xpcom) add_makefiles "$MAKEFILES_xpcom $MAKEFILES_libreg"
		;;
	    xpconnect) add_makefiles "
		$MAKEFILES_xpconnect $MAKEFILES_js $MAKEFILES_xpcom
		$MAKEFILES_libreg"
		;;
        esac
    done
fi
