#!/bin\sh
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
