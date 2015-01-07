dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Identify which version of the SDK we're building with
dnl Windows Server 2008 and newer SDKs have WinSDKVer.h, get the version
dnl from there
AC_DEFUN([MOZ_FIND_WINSDK_VERSION], [
  AC_CACHE_CHECK(for highest Windows version supported by this SDK,
                 ac_cv_winsdk_maxver,
                 [cat > conftest.h <<EOF
#include <winsdkver.h>

WINVER_MAXVER
EOF
                      ac_cv_winsdk_maxver=`$CPP conftest.h 2>/dev/null | tail -n1`
                      rm -f conftest.h
                     ])
      dnl WinSDKVer.h returns the version number in 4-digit format while many
      dnl consumers expect 8. Therefore, pad the result with an extra 4 zeroes.
      MOZ_WINSDK_MAXVER=${ac_cv_winsdk_maxver}0000
])
