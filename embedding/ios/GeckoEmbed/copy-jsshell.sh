#!/bin/bash

set -e

if test -z ${GECKO_OBJDIR}; then
    echo "Error: GECKO_OBJDIR not set!"
    exit 1
fi

if test "${ACTION}" != "clean"; then
    echo "Copying files from ${GECKO_OBJDIR}/dist/bin"
    mkdir -p $BUILT_PRODUCTS_DIR/$CONTENTS_FOLDER_PATH/Frameworks
    cp ${GECKO_OBJDIR}/mozglue/build/libmozglue.dylib $BUILT_PRODUCTS_DIR/$CONTENTS_FOLDER_PATH/Frameworks
    cp ${GECKO_OBJDIR}/config/external/nss/libnss3.dylib $BUILT_PRODUCTS_DIR/$CONTENTS_FOLDER_PATH/Frameworks

    if test ${ARCHS} == "armv7"; then
        for x in $BUILT_PRODUCTS_DIR/$CONTENTS_FOLDER_PATH/Frameworks/*.dylib; do
            echo "Signing $x"
            /usr/bin/codesign --force --sign "${EXPANDED_CODE_SIGN_IDENTITY}" --preserve-metadata=identifier,entitlements,resource-rules $x
        done
    fi
fi
