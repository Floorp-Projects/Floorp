#!/bin/sh

if [ $# -lt 1 ]; then
  echo update.sh "<release tag name>"
  exit 1
fi

hunspell_dir=`dirname $0`

tmpclonedir=$(mktemp -d)
git clone https://github.com/hunspell/hunspell --depth 1 --branch $1 ${tmpclonedir}
# Back up mozilla files
cp ${hunspell_dir}/src/moz.build ${tmpclonedir}/src/hunspell
cp ${hunspell_dir}/src/moz.yaml ${tmpclonedir}/src/hunspell
cp ${hunspell_dir}/src/sources.mozbuild ${tmpclonedir}/src/hunspell

rm -rf ${hunspell_dir}/src
cp -r ${tmpclonedir}/src/hunspell/ ${hunspell_dir}/src
cp ${tmpclonedir}/license.hunspell ${hunspell_dir}/src
cp ${tmpclonedir}/license.myspell ${hunspell_dir}/src
cp ${tmpclonedir}/README.md ${hunspell_dir}/src
rm ${hunspell_dir}/src/Makefile.am
rm ${hunspell_dir}/src/filemgr.cxx
rm ${hunspell_dir}/src/hunvisapi.h.in
rm ${hunspell_dir}/src/hunzip.cxx
rm ${hunspell_dir}/src/hunzip.hxx
rm ${hunspell_dir}/src/utf_info.hxx
rm -rf ${tmpclonedir}

cd ${hunspell_dir}/src
patch -p5 < ../patches/bug1410214.patch
patch -p5 < ../patches/bug1653659.patch
patch -p5 < ../patches/bug1739761.patch
