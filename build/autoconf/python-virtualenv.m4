dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_PYTHON],
[

dnl We honor the Python path defined in an environment variable. This is used
dnl to pass the virtualenv's Python from the main configure to SpiderMonkey's
dnl configure, for example.
if test -z "$PYTHON"; then
  MOZ_PATH_PROGS(PYTHON, $PYTHON python2.7 python)
  if test -z "$PYTHON"; then
      AC_MSG_ERROR([python was not found in \$PATH])
  fi
else
  AC_MSG_RESULT([Using Python from environment variable \$PYTHON])
fi

_virtualenv_topsrcdir=
_virtualenv_populate_path=

dnl If this is a mozilla-central, we'll find the virtualenv in the top
dnl source directory. If this is a SpiderMonkey build, we assume we're at
dnl js/src and try to find the virtualenv from the mozilla-central root.
for base in $MOZILLA_CENTRAL_PATH $_topsrcdir $_topsrcdir/../..; do
  possible=$base/build/virtualenv/populate_virtualenv.py

  if test -e $possible; then
    _virtualenv_topsrcdir=$base
    _virtualenv_populate_path=$possible
    break
  fi
done

if test -z $_virtualenv_populate_path; then
    AC_MSG_ERROR([Unable to find Virtualenv population script. In order
to build, you will need mozilla-central's virtualenv.

If you are building from a mozilla-central checkout, you should never see this
message. If you are building from a source archive, the source archive was
likely not created properly (it is missing the virtualenv files).

If you have a copy of mozilla-central available, define the
MOZILLA_CENTRAL_PATH environment variable to the top source directory of
mozilla-central and relaunch configure.])

fi

if test -z $DONT_POPULATE_VIRTUALENV; then
  AC_MSG_RESULT([Creating Python environment])
  dnl This verifies our Python version is sane and ensures the Python
  dnl virtualenv is present and up to date. It sanitizes the environment
  dnl for us, so we don't need to clean anything out.
  $PYTHON $_virtualenv_populate_path \
    $_virtualenv_topsrcdir $MOZ_BUILD_ROOT/_virtualenv || exit 1

  case "$host_os" in
  mingw*)
    PYTHON=`cd $MOZ_BUILD_ROOT && pwd -W`/_virtualenv/Scripts/python.exe
    ;;
  *)
    PYTHON=$MOZ_BUILD_ROOT/_virtualenv/bin/python
    ;;
  esac
fi

AC_SUBST(PYTHON)

AC_MSG_CHECKING([Python environment is Mozilla virtualenv])
$PYTHON -c "import mozbuild.base"
if test "$?" != 0; then
    AC_MSG_ERROR([Python environment does not appear to be sane.])
fi
AC_MSG_RESULT([yes])
])

