#!/bin/bash

# Find out ASAP if some command breaks here, because we're copying a lot of
# files we don't actually maintain ourselves, and requirements could easily be
# broken.
set -e

: ${MKDIR:=mkdir}
: ${TAR:=tar}
: ${RSYNC:=rsync}
: ${AUTOCONF:=$(which autoconf-2.13 autoconf2.13 autoconf213 | head -1)}
: ${SRCDIR:=$(cd $(dirname $0); pwd 2>/dev/null)}
: ${MOZJS_NAME:=mozjs}
# The place to gather files to be added to the tarball.
: ${STAGING:=/tmp/mozjs-src-pkg}
# The place to put the resulting tarball.
: ${DIST:=/tmp}

if [[ -f "$SRCDIR/../../config/milestone.txt" ]]; then
    MILESTONE="$(tail -1 $SRCDIR/../../config/milestone.txt)"
    IFS=. read -a VERSION < <(echo "$MILESTONE")
    MOZJS_MAJOR_VERSION=${MOZJS_MAJOR_VERSION:-${VERSION[0]}}
    MOZJS_MINOR_VERSION=${MOZJS_MINOR_VERSION:-${VERSION[1]}}
    MOZJS_PATCH_VERSION=${MOZJS_PATCH_VERSION:-${VERSION[2]}}
fi

cmd=${1:-build}
version="${MOZJS_NAME}-${MOZJS_MAJOR_VERSION}.${MOZJS_MINOR_VERSION}.${MOZJS_PATCH_VERSION:-${MOZJS_ALPHA:-0}}"
tgtpath=${STAGING}/${version}
pkg="${version}.tar.bz2"
pkgpath="${DIST}/${pkg}"
taropts="-jcf"

# need these environment vars:
echo "Environment:"
echo "    MAKE = $MAKE"
echo "    MKDIR = $MKDIR"
echo "    TAR = $TAR"
echo "    RSYNC = $RSYNC"
echo "    AUTOCONF = $AUTOCONF"
echo "    STAGING = $STAGING"
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
    echo "Cleaning ${pkgpath} and ${tgtpath} ..."
    rm -rf ${pkgpath} ${tgtpath}
    ;;
"build")
    # Make sure that everything copied here is kept in sync with
    # `taskcluster/ci/spidermonkey/kind.yml`!

    if [ -e ${tgtpath}/js/src/Makefile ]; then
        echo "error: found js/src/Makefile. Please clean before packaging." >&2
        exit 1
    fi

    echo "Staging source tarball in ${tgtpath}..."
    if [ -d ${tgtpath} ]; then
        echo "WARNING - dist tree ${tgtpath} already exists!"
    fi

    ${MKDIR} -p ${tgtpath}

    rsync \
        --delete-excluded \
        --prune-empty-dirs \
        --quiet \
        --recursive \
        "${TOPSRCDIR}"/ ${tgtpath}/ \
        --filter=". -" <<FILTER_EOF
# Top-level config and build files

+ /configure.py
+ /LICENSE
+ /Makefile.in
+ /moz.build
+ /moz.configure
+ /test.mozbuild

# Additional libraries (optionally) used by SpiderMonkey

+ /mfbt/**
+ /nsprpub/**

- /intl/icu/source/data
- /intl/icu/source/test
- /intl/icu/source/tools
+ /intl/icu/**

+ /memory/build/**
+ /memory/moz.build
+ /memory/mozalloc/**

+ /modules/fdlibm/**
+ /modules/zlib/**

+ /mozglue/baseprofiler/**
+ /mozglue/build/**
+ /mozglue/misc/**
+ /mozglue/moz.build
+ /mozglue/static/**

+ /tools/fuzzing/moz.build
+ /tools/fuzzing/interface/**
+ /tools/fuzzing/registry/**
+ /tools/fuzzing/libfuzzer/**

# Build system and dependencies

+ /Cargo.lock
+ /build/**
+ /config/**
+ /python/**

+ /.cargo/config.in

- /third_party/python/gyp
+ /third_party/python/**
+ /third_party/rust/**

+ /layout/tools/reftest/reftest/**

+ /testing/mozbase/**
+ /testing/web-platform/tests/streams/**

+ /toolkit/crashreporter/tools/symbolstore.py
+ /toolkit/mozapps/installer/package-name.mk

# SpiderMonkey itself

+ /js/src/**
+ /js/app.mozbuild
+ /js/*.configure
+ /js/examples/**
+ /js/public/**
+ /js/rust/**

+ */
- /**
FILTER_EOF

    # Copy Cargo.toml, filtering references to not-included directories out.
    cat ${TOPSRCDIR}/Cargo.toml \
        | grep -v security/manager/ssl/osclientcerts \
        | grep -v testing/geckodriver \
        | grep -v toolkit/crashreporter/rust \
        | grep -v toolkit/library/gtest/rust \
        | grep -v toolkit/library/rust \
        | grep -v dom/webauthn/libudev-sys \
        > ${tgtpath}/Cargo.toml

    # Generate configure files to avoid build dependency on autoconf-2.13
    cp -pPR ${TOPSRCDIR}/js/src/configure.in ${tgtpath}/js/src/configure
    chmod a+x ${tgtpath}/js/src/configure
    ${AUTOCONF} --localdir=${TOPSRCDIR}/js/src \
        ${TOPSRCDIR}/js/src/old-configure.in >${tgtpath}/js/src/old-configure

    # Copy or create INSTALL
    if [ -e ${STAGING}/INSTALL ]; then
        cp ${STAGING}/INSTALL ${tgtpath}/
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

    # Copy or create README
    if [ -e ${STAGING}/README ]; then
        cp ${STAGING}/README ${tgtpath}/
    else
        cat <<README_EOF >${tgtpath}/README
This directory contains SpiderMonkey ${MOZJS_MAJOR_VERSION}.

This release is based on a revision of Mozilla ${MOZJS_MAJOR_VERSION}:
  https://hg.mozilla.org/releases/
The changes in the patches/ directory were applied.

MDN hosts the latest SpiderMonkey ${MOZJS_MAJOR_VERSION} release notes:
  https://developer.mozilla.org/en-US/docs/SpiderMonkey/${MOZJS_MAJOR_VERSION}
README_EOF
    fi

    # Copy patches dir, if it currently exists in STAGING
    if [ -d ${STAGING}/patches ]; then
        cp -pPR ${STAGING}/patches ${tgtpath}/
    elif [ -d ${TOPSRCDIR}/patches ]; then
        cp -pPR ${TOPSRCDIR}/patches ${tgtpath}/
    fi

    # Remove *.pyc and *.pyo files if any
    find ${tgtpath} -type f -name "*.pyc" -o -name "*.pyo" | xargs rm -f

    # Roll the tarball
    echo "Packaging source tarball at ${pkgpath}..."
    ${TAR} $taropts ${pkgpath} -C ${STAGING} ${version}
    ;;
*)
    echo "Unrecognized command: $cmd"
    ;;
esac
