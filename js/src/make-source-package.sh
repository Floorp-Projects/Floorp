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
	echo "Packaging source tarball ${pkg}..."
	if [ -d ${tgtpath} ]; then
		echo "WARNING - dist tree ${tgtpath} already exists!"
	fi
	${MKDIR} -p ${tgtpath}/js/src

	# copy the embedded icu
	${MKDIR} -p ${tgtpath}/intl
	cp -t ${tgtpath}/intl -dRp ${SRCDIR}/../../intl/icu

	# copy autoconf config directory.
	${MKDIR} -p ${tgtpath}/build
	cp -t ${tgtpath}/build -dRp ${SRCDIR}/../../build/autoconf

	# put in js itself
	cp -t ${tgtpath} -dRp ${SRCDIR}/../../mfbt
	cp -t ${tgtpath}/js -dRp ${SRCDIR}/../jsd ${SRCDIR}/../public
	find ${SRCDIR} -mindepth 1 -maxdepth 1 -not -path ${DIST} -a -not -name ${pkg} \
		-exec cp -t ${tgtpath}/js/src -dRp {} +

	# distclean if necessary
	if [ -e ${tgtpath}/js/src/Makefile ]; then
		${MAKE} -C ${tgtpath}/js/src distclean
	fi

	# put in the virtualenv and supporting files if it doesnt already exist
	if [ ! -e ${SRCDIR}/build/virtualenv_packages.txt ]; then
		cp -t ${tgtpath}/js/src/build -dRp \
			${SRCDIR}/../../build/virtualenv_packages.txt \
			${SRCDIR}/../../build/buildconfig.py
	fi
	if [ ! -e ${SRCDIR}/python ]; then
		cp -t ${tgtpath}/js/src -dRp \
			${SRCDIR}/../../python
	fi
	if [ ! -e ${SRCDIR}/testing ]; then
		${MKDIR} -p ${tgtpath}/js/src/testing
		cp -t ${tgtpath}/js/src/testing -dRp \
			${SRCDIR}/../../testing/mozbase
	fi
	# end of virtualenv injection

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
