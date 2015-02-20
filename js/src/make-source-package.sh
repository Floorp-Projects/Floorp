#!/bin/sh

# Find out ASAP if some command breaks here, because we're copying a lot of
# files we don't actually maintain ourselves, and requirements could easily be
# broken.
set -e

# need these environment vars:
echo "Environment:"
echo "    MAKE = $MAKE"
echo "    MKDIR = $MKDIR"
echo "    TAR = $TAR"
echo "    DIST = $DIST"
echo "    SRCDIR = $SRCDIR"
echo "    MOZJS_MAJOR_VERSION = $MOZJS_MAJOR_VERSION"
echo "    MOZJS_MINOR_VERSION = $MOZJS_MINOR_VERSION"
echo "    MOZJS_PATCH_VERSION = $MOZJS_PATCH_VERSION"
echo "    MOZJS_ALPHA = $MOZJS_ALPHA"

cmd=${1:-build}
pkg="mozjs-${MOZJS_MAJOR_VERSION}.${MOZJS_MINOR_VERSION}.${MOZJS_PATCH_VERSION:-${MOZJS_ALPHA:-0}}.tar.bz2"
pkgpath=${pkg%.tar*}
tgtpath=${DIST}/${pkgpath}
taropts="-jcf"

case $cmd in
"clean")
	echo "Cleaning ${pkg} and ${tgtpath} ..."
	rm -rf ${pkg} ${tgtpath}
	;;
"build")
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
	cp -t ${tgtpath}/intl -dRp ${SRCDIR}/../../intl/icu

	# copy main moz.build and Makefile.in
	cp -t ${tgtpath} -dRp ${SRCDIR}/../../Makefile.in ${SRCDIR}/../../moz.build

	# copy a nspr file used by the build system
	${MKDIR} -p ${tgtpath}/nsprpub/config
	cp -t ${tgtpath}/nsprpub/config -dRp \
		${SRCDIR}/../../nsprpub/config/make-system-wrappers.pl

	# copy build and config directory.
	cp -t ${tgtpath} -dRp ${SRCDIR}/../../build ${SRCDIR}/../../config

	# put in js itself
	cp -t ${tgtpath} -dRp ${SRCDIR}/../../mfbt
	cp -t ${tgtpath}/js -dRp ${SRCDIR}/../public
	find ${SRCDIR} -mindepth 1 -maxdepth 1 -not -path ${DIST} -a -not -name ${pkg} \
		-exec cp -t ${tgtpath}/js/src -dRp {} +

	# distclean if necessary
	if [ -e ${tgtpath}/js/src/Makefile ]; then
		${MAKE} -C ${tgtpath}/js/src distclean
	fi

	cp -t ${tgtpath} -dRp \
		${SRCDIR}/../../python
	${MKDIR} -p ${tgtpath}/dom/bindings
	cp -t ${tgtpath}/dom/bindings -dRp \
		${SRCDIR}/../../dom/bindings/mozwebidlcodegen
	${MKDIR} -p ${tgtpath}/media/webrtc/trunk/tools
	cp -t ${tgtpath}/media/webrtc/trunk/tools -dRp \
		${SRCDIR}/../../media/webrtc/trunk/tools/gyp
	${MKDIR} -p ${tgtpath}/testing
	cp -t ${tgtpath}/testing -dRp \
		${SRCDIR}/../../testing/mozbase
	${MKDIR} -p ${tgtpath}/modules/zlib
	cp -t ${tgtpath}/modules/zlib -dRp \
		${SRCDIR}/../../modules/zlib/src
	${MKDIR} -p ${tgtpath}/layout/tools/reftest
	cp -t ${tgtpath}/layout/tools/reftest -dRp \
	        ${SRCDIR}/../../layout/tools/reftest/reftest
	${MKDIR} -p ${tgtpath}/toolkit/mozapps/installer
	cp -t ${tgtpath}/toolkit/mozapps/installer -dRp \
	        ${SRCDIR}/../../toolkit/mozapps/installer/package-name.mk \
	        ${SRCDIR}/../../toolkit/mozapps/installer/upload-files.mk \

	# remove *.pyc and *.pyo files if any
	find ${tgtpath} -type f -name "*.pyc" -o -name "*.pyo" |xargs rm -f

	# copy or create INSTALL
	if [ -e {DIST}/INSTALL ]; then
		cp -t ${tgtpath} ${DIST}/INSTALL
	else
		cat <<INSTALL_EOF >${tgtpath}/INSTALL
Full build documentation for SpiderMonkey is hosted on MDN:
  https://developer.mozilla.org/en-US/docs/SpiderMonkey/Build_Documentation

Note that the libraries produced by the build system include symbols,
causing the binaries to be extremely large. It is highly suggested that \`strip\`
be run over the binaries before deploying them.

Building with default options may be performed as follows:
  cd js/src
  ./configure
  make
INSTALL_EOF
	fi

	# copy or create README
	if [ -e ${DIST}/README ]; then
		cp -t ${tgtpath} ${DIST}/README
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
	if [ -e ${SRCDIR}/../../b2g/LICENSE ]; then
		cp ${SRCDIR}/../../b2g/LICENSE ${tgtpath}/
	else
		cp ${SRCDIR}/../../LICENSE ${tgtpath}/
	fi

	# copy patches dir, if it currently exists in DIST
	if [ -d ${DIST}/patches ]; then
		cp -t ${tgtpath} -dRp ${DIST}/patches
	elif [ -d ${SRCDIR}/../../patches ]; then
		cp -t ${tgtpath} -dRp ${SRCDIR}/../../patches
	fi

	# Roll the tarball
	${TAR} $taropts ${DIST}/../${pkg} -C ${DIST} ${pkgpath}
	echo "done."
	;;
*)
	echo "Unrecognized command: $cmd"
	;;
esac
