#!/bin\sh

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
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
# compver.sh - a script to check if the correct component version is
# available.  If it is not available, it uses the nsftp.sh script to
# download the component version.  The component release is assumed
# to be under /share/builds/components on a UNIX box.
#
COMP_ROOT=$1
COMP_VERSION=$2
COMP_VERSION_FILE=${COMP_ROOT}/Version
COMPOBJDIR=$3
MCOM_ROOT=$4
MODULE=$5		# Module which needs this component
COMP_RELEASE=$6		# Component release dir
COMP_NAME=$7		# component name (e.g. ldapsdk, rouge)
COMP_SUBDIRS=$8		# subdirs to ftp over
TEST_FILE=$9		# to test if ftp was successful

if test -r ${COMP_VERSION_FILE}; then \
  CUR_VERSION=`cat ${COMP_VERSION_FILE}`; \

  if test "${CUR_VERSION}" = "${COMP_VERSION}"; then \
    if test -d ${COMP_ROOT}/${COMPOBJDIR}; then \
      exit 0; \
    fi; \
  fi; \
fi

echo "************************ WARNING *************************"
echo "The MODULE ${MODULE} needs ${COMP_NAME} client libraries."
echo "The ${COMP_NAME} client libraries are missing.  "
echo ""
echo "Attempting to download..."

rm -rf ${COMP_ROOT}/${COMPOBJDIR} ${COMP_VERSION_FILE}
mkdir -p ${COMP_ROOT}/${COMPOBJDIR}

sh ../../build/nsftp.sh ${COMP_NAME}/${COMP_VERSION}/${COMPOBJDIR} ${COMP_ROOT}/${COMPOBJDIR}

for d in ${COMP_SUBDIRS}; do \
  mkdir -p ${COMP_ROOT}/${COMPOBJDIR}/${d}; \
  sh ../../build/nsftp.sh ${COMP_NAME}/${COMP_VERSION}/${COMPOBJDIR}/${d} ${COMP_ROOT}/${COMPOBJDIR}/${d}
done

if test -f ${TEST_FILE}; then \
    echo "${COMP_VERSION}" > ${COMP_VERSION_FILE}; \
    echo "************************ SUCCESS! ************************"; \
else \
    echo ""; \
    echo "Attempt to ftp over ${COMP_NAME} failed!!!"; \
    echo "Please ftp over (${COMP_SUBDIRS}) subdirectories under:";  \
    echo "    ${COMP_RELEASE}"; \
    echo "and put them under:";  \
    echo "    ${COMP_ROOT}/${COMPOBJDIR}";  \
    echo "Also, execute the following command: "; \
    echo "     echo \"${COMP_VERSION}\" > ${COMP_VERSION_FILE}"; \
    echo "Note: Above directories are w.r.t. the MODULE ${MODULE}";  \
    echo "**********************************************************";  \
    exit 1; \
fi
