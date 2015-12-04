#!/bin/bash

# Find out ASAP if some command breaks here, because we're copying a lot of
# files we don't actually maintain ourselves, and requirements could easily be
# broken.
set -e

: ${MAKE:=make}
: ${MKDIR:=mkdir}
: ${TAR:=tar}
: ${SRCDIR:=$(cd $(dirname $0); pwd 2>/dev/null)}
: ${MOZJS_NAME:=mozjs}
: ${DIST:=/tmp/mozjs-src-pkg}

if [[ -f "$SRCDIR/../../config/milestone.txt" ]]; then
	MILESTONE="$(tail -1 $SRCDIR/../../config/milestone.txt)"
	IFS=. read -a VERSION < <(echo "$MILESTONE")
	MOZJS_MAJOR_VERSION=${MOZJS_MAJOR_VERSION:-${VERSION[0]}}
	MOZJS_MINOR_VERSION=${MOZJS_MINOR_VERSION:-${VERSION[1]}}
	MOZJS_PATCH_VERSION=${MOZJS_PATCH_VERSION:-${VERSION[2]}}
fi

cmd=${1:-build}
pkg="${MOZJS_NAME}-${MOZJS_MAJOR_VERSION}.${MOZJS_MINOR_VERSION}.${MOZJS_PATCH_VERSION:-${MOZJS_ALPHA:-0}}.tar.bz2"
pkgpath=${pkg%.tar*}
tgtpath=${DIST}/${pkgpath}
taropts="-jcf"

# need these environment vars:
echo "Environment:"
echo "    MAKE = $MAKE"
echo "    MKDIR = $MKDIR"
echo "    TAR = $TAR"
echo "    DIST = $DIST"
echo "    SRCDIR = $SRCDIR"
echo "    MOZJS_NAME = $MOZJS_NAME"
echo "    MOZJS_MAJOR_VERSION = $MOZJS_MAJOR_VERSION"
echo "    MOZJS_MINOR_VERSION = $MOZJS_MINOR_VERSION"
echo "    MOZJS_PATCH_VERSION = $MOZJS_PATCH_VERSION"
echo "    MOZJS_ALPHA = $MOZJS_ALPHA"
echo ""

TOPSRCDIR=${SRCDIR}/../..

case $cmd in
"clean")
	echo "Cleaning ${pkg} and ${tgtpath} ..."
	rm -rf ${pkg} ${tgtpath}
	;;
"build")
	echo -n "Press enter to build $pkg> "
	read

	# Ensure that the configure script is newer than the configure.in script.
	if [ ${SRCDIR}/configure.in -nt ${SRCDIR}/configure ]; then
		echo "error: js/src/configure is out of date. Please regenerate before packaging." >&2
		exit 1
	fi

	echo "Packaging source tarball ${pkg}..."
	if [ -d ${tgtpath} ]; then
		echo "WARNING - dist tree ${tgtpath} already exists!"
	fi
	${MKDIR} -p ${tgtpath}/js/src

	# copy the embedded icu
	${MKDIR} -p ${tgtpath}/intl
	cp -pPR ${TOPSRCDIR}/intl/icu ${tgtpath}/intl

	# copy main moz.build and Makefile.in
	cp -pPR ${TOPSRCDIR}/Makefile.in ${TOPSRCDIR}/moz.build ${tgtpath}

	# copy nspr.
	cp -pPR ${SRCDIR}/../../nsprpub ${tgtpath}

	# copy build and config directory.
	cp -pPR ${TOPSRCDIR}/build ${TOPSRCDIR}/config ${tgtpath}

	# put in js itself
	cp -pPR ${TOPSRCDIR}/mfbt ${tgtpath}
	cp -pPR ${SRCDIR}/../public ${tgtpath}/js
	find ${SRCDIR} -mindepth 1 -maxdepth 1 -not -path ${DIST} -a -not -name ${pkg} \
		-exec cp -pPR {} ${tgtpath}/js/src \;

	# distclean if necessary
	if [ -e ${tgtpath}/js/src/Makefile ]; then
		${MAKE} -C ${tgtpath}/js/src distclean
	fi

	cp -pPR \
		${TOPSRCDIR}/python \
		${tgtpath}
	${MKDIR} -p ${tgtpath}/dom/bindings
	cp -pPR \
		${TOPSRCDIR}/dom/bindings/mozwebidlcodegen \
		${tgtpath}/dom/bindings
	${MKDIR} -p ${tgtpath}/media/webrtc/trunk/tools
	cp -pPR \
		${TOPSRCDIR}/media/webrtc/trunk/tools/gyp \
		${tgtpath}/media/webrtc/trunk/tools
	${MKDIR} -p ${tgtpath}/testing
	cp -pPR \
		${TOPSRCDIR}/testing/mozbase \
		${tgtpath}/testing
	${MKDIR} -p ${tgtpath}/modules
	cp -pPR \
		${TOPSRCDIR}/modules/zlib/src \
		${tgtpath}/modules
	${MKDIR} -p ${tgtpath}/layout/tools/reftest
	cp -pPR \
		${TOPSRCDIR}/layout/tools/reftest/reftest \
		${tgtpath}/layout/tools/reftest
	${MKDIR} -p ${tgtpath}/toolkit/mozapps/installer
	cp -pPR \
		${TOPSRCDIR}/toolkit/mozapps/installer/package-name.mk \
		${TOPSRCDIR}/toolkit/mozapps/installer/upload-files.mk \
		${tgtpath}/toolkit/mozapps/installer
	${MKDIR} -p ${tgtpath}/mozglue
	cp -pPR \
		${TOPSRCDIR}/mozglue/build \
		${TOPSRCDIR}/mozglue/crt \
		${TOPSRCDIR}/mozglue/misc \
		${TOPSRCDIR}/mozglue/moz.build \
		${tgtpath}/mozglue
	${MKDIR} -p ${tgtpath}/memory
	cp -pPR \
		${TOPSRCDIR}/memory/moz.build \
		${TOPSRCDIR}/memory/build \
		${TOPSRCDIR}/memory/fallible \
		${TOPSRCDIR}/memory/jemalloc \
		${TOPSRCDIR}/memory/mozalloc \
		${TOPSRCDIR}/memory/mozjemalloc \
		${tgtpath}/memory

	# remove *.pyc and *.pyo files if any
	find ${tgtpath} -type f -name "*.pyc" -o -name "*.pyo" |xargs rm -f

	# copy or create INSTALL
	if [ -e ${DIST}/INSTALL ]; then
		cp ${DIST}/INSTALL ${tgtpath}
	else
		cat <<INSTALL_EOF >${tgtpath}/INSTALL
Full build documentation for SpiderMonkey is hosted on MDN:
  https://developer.mozilla.org/en-US/docs/SpiderMonkey/Build_Documentation

Note that the libraries produced by the build system include symbols,
causing the binaries to be extremely large. It is highly suggested that \`strip\`
be run over the binaries before deploying them.

Building with default options may be performed as follows:
  cd js/src
  mkdir obj
  cd obj
  ../configure
  make # or mozmake on Windows
INSTALL_EOF
	fi

	# copy or create README
	if [ -e ${DIST}/README ]; then
		cp ${DIST}/README ${tgtpath}
	else
		cat <<README_EOF >${tgtpath}/README
This directory contains SpiderMonkey ${MOZJS_MAJOR_VERSION}.

This release is based on a revision of Mozilla ${MOZJS_MAJOR_VERSION}:
  http://hg.mozilla.org/releases/
The changes in the patches/ directory were applied.

MDN hosts the latest SpiderMonkey ${MOZJS_MAJOR_VERSION} release notes:
  https://developer.mozilla.org/en-US/docs/SpiderMonkey/${MOZJS_MAJOR_VERSION}
README_EOF
	fi

	# copy LICENSE
	if [ -e ${TOPSRCDIR}/b2g/LICENSE ]; then
		cp ${TOPSRCDIR}/b2g/LICENSE ${tgtpath}/
	else
		cp ${TOPSRCDIR}/LICENSE ${tgtpath}/
	fi

	# copy patches dir, if it currently exists in DIST
	if [ -d ${DIST}/patches ]; then
		cp -pPR ${DIST}/patches ${tgtpath}
	elif [ -d ${TOPSRCDIR}/patches ]; then
		cp -pPR ${TOPSRCDIR}/patches ${tgtpath}
	fi

	# Roll the tarball
	${TAR} $taropts ${DIST}/../${pkg} -C ${DIST} ${pkgpath}
	echo "Wrote $(cd ${DIST}/..; echo $PWD)/${pkg}"
	;;
*)
	echo "Unrecognized command: $cmd"
	;;
esac
