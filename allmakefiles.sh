#! /bin/sh
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation.  All Rights
# Reserved.
#

# allmakefiles.sh - List of all makefiles. 
#   Appends the list of makefiles to the variable, MAKEFILES.
#   There is no need to rerun autoconf after adding makefiles.
#   You only need to run configure.
#
#   Unused makefiles may be commented out with '#'.
#   ('#' must be the first character on the line).

# add_makefiles - Shell function to add makefiles to MAKEFILES
add_makefiles() {
  while read line; do
    case $line in
      \#*|dnl*) ;;
      *) MAKEFILES="$MAKEFILES $line" ;;
    esac
  done
}

if [ -z "${srcdir}" ]; then
    srcdir=.
fi

add_makefiles <<END_NGMAKEFILES
Makefile
build/Makefile
build/unix/Makefile
build/unix/nspr_my_config.mk
build/unix/nspr_my_overrides.mk
build/package/rpm/mozilla.spec
build/package/rpm/nspr-pthreads.spec
config/Makefile
config/autoconf.mk
config/mkdepend/Makefile
config/mkdetect/Makefile
dbm/Makefile
dbm/include/Makefile
dbm/src/Makefile
dbm/tests/Makefile
dom/Makefile
dom/public/Makefile
dom/public/base/Makefile
dom/public/coreDom/Makefile
dom/public/coreEvents/Makefile
dom/public/css/Makefile
dom/public/events/Makefile
dom/public/range/Makefile
dom/public/html/Makefile
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
editor/Makefile
editor/public/Makefile
editor/idl/Makefile
editor/txmgr/Makefile
editor/txmgr/public/Makefile
editor/txmgr/src/Makefile
editor/txmgr/tests/Makefile
editor/txtsvc/Makefile
editor/txtsvc/public/Makefile
editor/txtsvc/src/Makefile
expat/Makefile
expat/xmlparse/Makefile
expat/xmltok/Makefile
extensions/Makefile
extensions/wallet/Makefile
extensions/wallet/public/Makefile
extensions/wallet/src/Makefile
extensions/wallet/editor/Makefile
extensions/wallet/cookieviewer/Makefile
extensions/wallet/signonviewer/Makefile
extensions/wallet/walletpreview/Makefile
extensions/wallet/build/Makefile
extensions/pics/Makefile
extensions/pics/public/Makefile
extensions/pics/src/Makefile
extensions/pics/tests/Makefile
gfx/Makefile
gfx/public/Makefile
gfx/src/Makefile
gfx/src/beos/Makefile
gfx/src/gtk/Makefile
gfx/src/ps/Makefile
gfx/src/motif/Makefile
gfx/src/photon/Makefile
gfx/src/rhapsody/Makefile
gfx/src/qt/Makefile
gfx/src/xlib/Makefile
gfx/src/xlibrgb/Makefile
gfx/tests/Makefile
htmlparser/Makefile
htmlparser/robot/Makefile
htmlparser/src/Makefile
htmlparser/tests/Makefile
htmlparser/tests/grabpage/Makefile
htmlparser/tests/logparse/Makefile
include/Makefile
intl/Makefile
intl/chardet/Makefile
intl/chardet/public/Makefile
intl/chardet/src/Makefile
intl/uconv/Makefile
intl/uconv/public/Makefile
intl/uconv/src/Makefile
intl/uconv/tests/Makefile
intl/uconv/ucvja/Makefile
intl/uconv/ucvlatin/Makefile
intl/uconv/ucvcn/Makefile
intl/uconv/ucvtw/Makefile
intl/uconv/ucvtw2/Makefile
intl/uconv/ucvko/Makefile
intl/locale/Makefile
intl/locale/public/Makefile
intl/locale/idl/Makefile
intl/locale/src/Makefile
intl/locale/src/unix/Makefile
intl/locale/tests/Makefile
intl/lwbrk/Makefile
intl/lwbrk/src/Makefile
intl/lwbrk/public/Makefile
intl/lwbrk/tests/Makefile
intl/unicharutil/Makefile
intl/unicharutil/src/Makefile
intl/unicharutil/public/Makefile
intl/unicharutil/tests/Makefile
intl/unicharutil/tools/Makefile
intl/strres/Makefile
intl/strres/public/Makefile
intl/strres/src/Makefile
intl/strres/tests/Makefile
jpeg/Makefile
js/Makefile
#js/jsd/Makefile
#js/jsd/classes/Makefile
js/src/Makefile
js/src/fdlibm/Makefile
js/src/liveconnect/Makefile
js/src/liveconnect/classes/Makefile
# js/src/xpcom/Makefile
js/src/xpconnect/Makefile
js/src/xpconnect/public/Makefile
js/src/xpconnect/idl/Makefile
js/src/xpconnect/shell/Makefile
js/src/xpconnect/src/Makefile
js/src/xpconnect/tests/Makefile
js/src/xpconnect/tests/components/Makefile
js/src/xpconnect/shell/Makefile
# js/src/xpconnect/md/Makefile
# js/src/xpconnect/md/unix/Makefile
# js/src/xpconnect/test/Makefile
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
# lib/liblayer/Makefile
# lib/liblayer/include/Makefile
# lib/liblayer/src/Makefile
lib/libpwcac/Makefile
lib/xp/Makefile
modules/libimg/Makefile
# modules/libimg/classes/Makefile
# modules/libimg/classes/netscape/Makefile
# modules/libimg/classes/netscape/libimg/Makefile
modules/libimg/png/Makefile
modules/libimg/public/Makefile
modules/libimg/public_com/Makefile
modules/libimg/src/Makefile
modules/libimg/gifcom/Makefile
modules/libimg/jpgcom/Makefile
modules/libimg/pngcom/Makefile  
modules/libjar/Makefile
modules/libpref/Makefile
modules/libpref/admin/Makefile
modules/libpref/l10n/Makefile
modules/libpref/public/Makefile
modules/libpref/src/Makefile
modules/libreg/Makefile
modules/libreg/include/Makefile
modules/libreg/src/Makefile
modules/libutil/Makefile
modules/libutil/public/Makefile
modules/libutil/src/Makefile
modules/oji/Makefile
modules/oji/public/Makefile
modules/oji/src/Makefile
modules/plugin/Makefile
modules/plugin/nglsrc/Makefile
modules/plugin/public/Makefile
modules/plugin/src/Makefile
modules/plugin/test/Makefile
modules/security/freenav/Makefile
modules/zlib/Makefile
modules/zlib/src/Makefile
nav-java/Makefile
nav-java/stubs/Makefile
nav-java/stubs/include/Makefile
nav-java/stubs/jri/Makefile
nav-java/stubs/src/Makefile
network/Makefile
network/cache/Makefile
network/cache/nu/Makefile
network/cache/nu/include/Makefile
network/cache/nu/public/Makefile
network/cache/nu/src/Makefile
network/cache/nu/tests/Makefile
network/cache/nu/tests/fftest/Makefile
network/client/Makefile
network/cnvts/Makefile
network/cstream/Makefile
network/main/Makefile
network/mimetype/Makefile
network/public/Makefile
network/module/Makefile
network/module/tests/Makefile
network/protocol/Makefile
network/protocol/about/Makefile
network/protocol/callback/Makefile
network/protocol/dataurl/Makefile
network/protocol/file/Makefile
network/protocol/ftp/Makefile
network/protocol/gopher/Makefile
network/protocol/http/Makefile
network/protocol/js/Makefile
network/protocol/ldap/Makefile
network/protocol/marimba/Makefile
network/protocol/remote/Makefile
network/protocol/sockstub/Makefile
network/util/Makefile
profile/Makefile
profile/src/Makefile
profile/public/Makefile
profile/resources/Makefile
profile/defaults/Makefile
profile/idlservices/Makefile
rdf/Makefile
rdf/base/Makefile
rdf/base/idl/Makefile
rdf/base/public/Makefile
rdf/base/src/Makefile
rdf/brprof/Makefile
rdf/brprof/public/Makefile
rdf/brprof/src/Makefile
rdf/brprof/build/Makefile
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
sun-java/Makefile
sun-java/stubs/Makefile
sun-java/stubs/include/Makefile
sun-java/stubs/jri/Makefile
sun-java/stubs/src/Makefile
caps/Makefile
caps/idl/Makefile
caps/include/Makefile
# caps/public/Makefile
caps/src/Makefile
view/Makefile
view/public/Makefile
view/src/Makefile
webshell/Makefile
webshell/public/Makefile
webshell/src/Makefile
webshell/tests/Makefile
webshell/tests/viewer/Makefile
webshell/tests/viewer/public/Makefile
webshell/embed/Makefile
widget/Makefile
widget/public/Makefile
widget/src/Makefile
widget/src/beos/Makefile
widget/src/build/Makefile
widget/src/gtk/Makefile
widget/src/motif/Makefile
widget/src/motif/app_context/Makefile
widget/src/photon/Makefile
widget/src/rhapsody/Makefile
widget/src/unix_services/Makefile
widget/src/unix_services/toolkit_service/Makefile
widget/src/xlib/Makefile
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
widget/timer/src/photon/Makefile
widget/timer/src/rhapsody/Makefile
widget/timer/src/unix/Makefile
widget/timer/src/unix/gtk/Makefile
widget/timer/src/unix/motif/Makefile
widget/timer/src/unix/xlib/Makefile
widget/timer/src/unix/qt/Makefile
#
# New xpcom hiearchy
#
xpcom/Makefile
xpcom/base/Makefile
xpcom/build/Makefile
xpcom/components/Makefile
xpcom/ds/Makefile
# xpcom/idl/Makefile
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
# xpcom/public/Makefile
xpcom/sample/Makefile
# xpcom/src/Makefile
xpcom/tests/Makefile
xpcom/tests/dynamic/Makefile
xpcom/tests/services/Makefile
xpcom/threads/Makefile
xpcom/tools/Makefile
xpcom/tools/registry/Makefile
#
# End of xpcom Makefiles
#
silentdl/Makefile
xpinstall/Makefile
xpinstall/notifier/Makefile
xpinstall/packager/Makefile
xpinstall/public/Makefile
xpinstall/res/Makefile
xpinstall/src/Makefile
xpfe/Makefile
xpfe/AppCores/Makefile
xpfe/AppCores/public/Makefile
xpfe/AppCores/src/Makefile
xpfe/AppCores/xul/Makefile
xpfe/AppCores/idl/Makefile
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
xpfe/components/bookmarks/Makefile
xpfe/components/bookmarks/public/Makefile
xpfe/components/bookmarks/src/Makefile
xpfe/components/bookmarks/resources/Makefile
xpfe/components/find/Makefile
xpfe/components/find/public/Makefile
xpfe/components/find/src/Makefile
xpfe/components/find/resources/Makefile
xpfe/components/history/Makefile
xpfe/components/history/src/Makefile
xpfe/components/history/public/Makefile
xpfe/components/history/resources/Makefile
xpfe/components/prefwindow/Makefile
xpfe/components/prefwindow/src/Makefile
xpfe/components/prefwindow/public/Makefile
xpfe/components/prefwindow/resources/Makefile
xpfe/components/prefwindow/resources/content/Makefile
xpfe/components/prefwindow/resources/skin/Makefile
xpfe/components/prefwindow/resources/locale/Makefile
xpfe/components/prefwindow/resources/locale/en-US/Makefile
xpfe/components/related/Makefile
xpfe/components/related/src/Makefile
xpfe/components/related/public/Makefile
xpfe/components/related/resources/Makefile
xpfe/components/search/Makefile
xpfe/components/search/datasets/Makefile
xpfe/components/search/resources/Makefile
xpfe/components/sidebar/Makefile
xpfe/components/sidebar/resources/Makefile
xpfe/components/xfer/Makefile
xpfe/components/xfer/public/Makefile
xpfe/components/xfer/src/Makefile
xpfe/components/xfer/resources/Makefile
xpfe/components/ucth/Makefile
xpfe/components/ucth/public/Makefile
xpfe/components/ucth/src/Makefile
xpfe/components/ucth/resources/Makefile
xpfe/appshell/Makefile
xpfe/appshell/src/Makefile
xpfe/appshell/public/Makefile
xpfe/bootstrap/Makefile
xpfe/browser/Makefile
xpfe/browser/src/Makefile
xpfe/browser/resources/Makefile
xpfe/browser/resources/content/Makefile
xpfe/browser/resources/skin/Makefile
xpfe/browser/resources/locale/Makefile
xpfe/browser/resources/locale/en-US/Makefile
xpfe/global/Makefile
xpfe/global/resources/Makefile
xpfe/global/resources/skin/Makefile
xpfe/global/resources/content/Makefile
xpfe/global/resources/locale/Makefile

# xpfe/browser/public/Makefile
END_NGMAKEFILES

if [ "$MOZ_EDITOR" ]; then
  add_makefiles <<END_EDITOR_MAKEFILES
editor/base/Makefile
editor/ui/Makefile
editor/ui/composer/Makefile
editor/ui/composer/content/Makefile
editor/ui/composer/skin/Makefile
editor/ui/composer/locale/Makefile
editor/ui/composer/locale/en-US/Makefile
editor/ui/dialogs/Makefile
editor/ui/dialogs/content/Makefile
editor/ui/dialogs/skin/Makefile
editor/ui/dialogs/locale/Makefile
editor/ui/dialogs/locale/en-US/Makefile

END_EDITOR_MAKEFILES
fi

if [ "$MOZ_MAIL_NEWS" ]; then
  add_makefiles < ${srcdir}/mailnews/makefiles
fi

if [ "$NECKO" ]; then
  add_makefiles < ${srcdir}/netwerk/makefiles
  add_makefiles <<END_NECKO_MAKEFILES
  extensions/cookie/Makefile
  extensions/cookie/tests/Makefile
  htmlparser/robot/test/Makefile
END_NECKO_MAKEFILES
fi
