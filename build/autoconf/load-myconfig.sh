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

# load-myconfig.sh - Loads options from myconfig.sh onto configure's
#    command-line. The myconfig.sh is searched for in the following
#    order: $MOZ_MYCONFIG, $objdir/myconfig.sh, $topsrcdir/myconfig.sh.
#
#    The options from myconfig.sh are inserted into the command-line
#    before the real command-line options. This way the real options
#    will override any myconfig.sh options.
#
# myconfig.sh is a shell script. To add an option to configure's
#    command-line use the pre-defined function, ac_add_options,
#
#       ac_add_options  <configure-option> [<configure-option> ... ]
#
#    For example,
#
#       ac_add_options --with-pthreads --enable-debug
#
# ac_add_options can be called multiple times in myconfig.sh.
#    Each call adds more options to configure's command-line.
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).


ac_add_options() {
  for _opt
  do
    # Escape shell characters, space, tab, dollar, quote, backslash.
    _opt=`echo $_opt | sed -e 's/\([\ \	\$\"\\]\)/\\\\\1/g;'`

    config_file_options="$config_file_options $_opt"
  done
}

ac_echo_options() {
  echo "Adding options from `pwd`/myconfig.sh:"
  eval "set -- $config_file_options"
  for opt
  do
    echo "  $opt"
  done
}

#
# Define load the options
#
ac_options=
config_file_options=

# Save the real command-line options
for _opt
do
  # Escape shell characters, space, tab, dollar, quote, backslash.
  _opt=`echo $_opt | sed -e 's/\([\ \	\$\"\\]\)/\\\\\1/g;'`

  ac_options="$ac_options \"$_opt\""
done

. $MOZ_MYCONFIG

ac_echo_options 1>&2

eval "set -- $config_file_options $ac_options"

