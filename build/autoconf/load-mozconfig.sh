# sh
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

# load-mozconfig.sh - Loads options from .mozconfig onto configure's
#    command-line. See find-mozconfig.sh for how the config file is
#    found
#
#    The options from .mozconfig are inserted into the command-line
#    before the real command-line options. This way the real options
#    will override any .mozconfig options.
#
# .mozconfig is a shell script. To add an option to configure's
#    command-line use the pre-defined function, ac_add_options,
#
#       ac_add_options  <configure-option> [<configure-option> ... ]
#
#    For example,
#
#       ac_add_options --with-pthreads --enable-debug
#
# ac_add_options can be called multiple times in .mozconfig.
#    Each call adds more options to configure's command-line.
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).


ac_add_options() {
  for _opt
  do
    # Escape shell characters, space, tab, dollar, quote, backslash.
    _opt=`echo $_opt | sed -e 's/\([\ \	\$\"\\]\)/\\\\\1/g;s/@\([^@]*\)@/\$\1/g;'`
    _opt=`echo $_opt | sed -e 's/@\([^@]*\)@/\$(\1)/g'`

    # Avoid adding duplicates
    case "$ac_options" in
      *"$_opt"* ) ;;
      * ) mozconfig_ac_options="$mozconfig_ac_options $_opt" ;;
    esac
  done
}

mk_add_options() {
  # These options are for client.mk
  # configure can safely ignore them.
  :
}

ac_echo_options() {
  echo "Adding options from $MOZCONFIG:"
  eval "set -- $mozconfig_ac_options"
  for _opt
  do
    echo "  $_opt"
  done
}

#
# Define load the options
#
ac_options=
mozconfig_ac_options=

# Save the real command-line options
for _opt
do
  # Escape shell characters, space, tab, dollar, quote, backslash.
  _opt=`echo $_opt | sed -e 's/\([\ \	\$\"\\]\)/\\\\\1/g;'`

  ac_options="$ac_options \"$_opt\""
done

# Call find-mozconfig.sh 
#   In params:   $MOZCONFIG $HOME (old:$MOZ_MYCONFIG)
_topsrcdir=`dirname $0`
MOZCONFIG=`$_topsrcdir/build/autoconf/find-mozconfig.sh`

if [ "$MOZCONFIG" ]; then
  . $MOZCONFIG
  ac_echo_options 1>&2
fi

eval "set -- $mozconfig_ac_options $ac_options"

