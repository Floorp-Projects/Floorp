#!/usr/bin/sh
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
# The Original Code is Mozilla.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Tao Cheng <tao@netscape.com>
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
#
#
# Author: Tao Cheng <tao@netscape.com>
# Date  : July 22, 2002
#
# use to this script to leverage transation from localized jar, xx-XX_jar, into the US embed_jar/
toLoc=$1
if [ "$toLoc" = "" ]
then
    echo "Usage: $0 toLocale"
    exit 1
fi
# leverage
subsKeyVal.pl embed_jar/locale/en-US/communicator/wallet/wallet.properties ${toLoc}_jar/locale/${toLoc}/communicator/wallet/wallet.properties ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/global/ldapAutoCompErrs.properties ${toLoc}_jar/locale/${toLoc}/global/ldapAutoCompErrs.properties ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/global/printing.properties ${toLoc}_jar/locale/${toLoc}/global/printing.properties ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/global/regionNames.properties ${toLoc}_jar/locale/${toLoc}/global/regionNames.properties ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/global/tabbrowser.dtd ${toLoc}_jar/locale/${toLoc}/global/tabbrowser.dtd ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/navigator/navigator.dtd ${toLoc}_jar/locale/${toLoc}/navigator/navigator.dtd ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/navigator/pageInfo.dtd ${toLoc}_jar/locale/${toLoc}/navigator/pageInfo.dtd ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/navigator/navigator.properties ${toLoc}_jar/locale/${toLoc}/navigator/navigator.properties ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/navigator/turboDialog.dtd ${toLoc}_jar/locale/${toLoc}/navigator/turboDialog.dtd ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/pipnss/pipnss.properties ${toLoc}_jar/locale/${toLoc}/pipnss/pipnss.properties ${toLoc}
subsKeyVal.pl embed_jar/locale/en-US/pippki/PageInfoOverlay.dtd ${toLoc}_jar/locale/${toLoc}/pippki/PageInfoOverlay.dtd ${toLoc}
#
# copying
cp embed_jar/locale/en-US/communicator/wallet/wallet.properties.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/communicator/wallet/wallet.properties
cp embed_jar/locale/en-US/global/ldapAutoCompErrs.properties.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/global/ldapAutoCompErrs.properties
cp embed_jar/locale/en-US/global/printing.properties.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/global/printing.properties
cp embed_jar/locale/en-US/global/regionNames.properties.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/global/regionNames.properties
cp embed_jar/locale/en-US/global/tabbrowser.dtd.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/global/tabbrowser.dtd
cp embed_jar/locale/en-US/navigator/navigator.dtd.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/navigator/navigator.dtd
cp embed_jar/locale/en-US/navigator/pageInfo.dtd.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/navigator/pageInfo.dtd
cp embed_jar/locale/en-US/navigator/navigator.properties.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/navigator/navigator.properties
cp embed_jar/locale/en-US/navigator/turboDialog.dtd.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/navigator/turboDialog.dtd
cp embed_jar/locale/en-US/pipnss/pipnss.properties.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/pipnss/pipnss.properties
cp embed_jar/locale/en-US/pippki/PageInfoOverlay.dtd.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/pippki/PageInfoOverlay.dtd
