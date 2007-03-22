#!/bin/sh
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
# The Original Code is the make-diskimage script.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Brian Ryner <bryner@brianryner.com>
#  Jean-Jacques Enser <jj@netscape.com>
#  Arthur Wiebe <artooro@gmail.com>
#  Mark Mentovai <mark@moxienet.com>
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

# Create a read-only disk image of the contents of a folder
#
# Usage: make-diskimage <image_file>
#                       <src_folder>
#                       <volume_name>
#                       <eula_resource_file>
#                       <.dsstore_file>
#                       <background_image_file>
#
# tip: use '-null-' for <eula-resource-file> if you only want to
# provide <.dsstore_file> and <background_image_file>

DMG_PATH=$1
SRC_FOLDER=$2
VOLUME_NAME=$3

# optional arguments
EULA_RSRC=$4
DMG_DSSTORE=$5
DMG_BKGND_IMG=$6

EXTRA_ARGS=

if test -n "$EULA_RSRC" && test "$EULA_RSRC" != "-null-" ; then
  EXTRA_ARGS="--resource $EULA_RSRC"
fi

if test -n "$DMG_DSSTORE" ; then
  EXTRA_ARGS="$EXTRA_ARGS --copy $DMG_DSSTORE:/.DS_Store"
fi

if test -n "$DMG_BKGND_IMG" ; then
  EXTRA_ARGS="$EXTRA_ARGS --mkdir /.background --copy $DMG_BKGND_IMG:/.background"
fi

echo `dirname $0`/pkg-dmg --target "$DMG_PATH" --source "$SRC_FOLDER" \
 --volname "$VOLUME_NAME" $EXTRA_ARGS

`dirname $0`/pkg-dmg --target "$DMG_PATH" --source "$SRC_FOLDER" \
 --volname "$VOLUME_NAME" $EXTRA_ARGS

exit $?
