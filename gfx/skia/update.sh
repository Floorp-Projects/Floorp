# Usage: ./update.sh <skia_src_directory>
#
# Copies the needed files from a directory containing the original
# skia source that we need.

cp $1/LICENSE ./
cp $1/README ./

# Includes
cp $1/include/animator/*.h ./include/animator/
cp $1/include/config/*.h ./include/config/
cp $1/include/core/*.h ./include/core/
cp $1/include/effects/*.h ./include/effects/
cp $1/include/gpu/*.h ./include/gpu/
cp $1/include/images/*.h ./include/images/
cp $1/include/pdf/*.h ./include/pdf/
cp $1/include/pipe/*.h ./include/pipe/
cp $1/include/ports/*.h ./include/ports/
cp $1/include/svg/*.h ./include/svg/
cp $1/include/text/*.h ./include/text/
cp $1/include/utils/android/*.h ./include/utils/android/
cp $1/include/utils/mac/*.h ./include/utils/mac/
cp $1/include/utils/unix/*.h ./include/utils/unix/
cp $1/include/utils/win/*.h ./include/utils/win/
cp $1/include/utils/*.h ./include/utils/
cp $1/include/views/*.h ./include/views/
cp $1/include/xml/*.h ./include/xml/

# Source
cp $1/src/animator/*.h ./src/animator/
cp $1/src/animator/*.xsd ./src/animator/
cp $1/src/animator/*.xsx ./src/animator/
cp $1/src/animator/*.cpp ./src/animator/
cp $1/src/core/*.h ./src/core/
cp $1/src/core/*.cpp ./src/core/
cp $1/src/effects/*.cpp ./src/effects/
cp $1/src/effects/*.h ./src/effects/
cp $1/src/gpu/*.cpp ./src/gpu/
cp $1/src/gpu/*.h ./src/gpu/
cp $1/src/images/*.cpp ./src/images/
cp $1/src/images/*.h ./src/images/
cp $1/src/opts/*.S ./src/opts/
cp $1/src/opts/*.cpp ./src/opts/
cp $1/src/opts/*.h ./src/opts/
cp $1/src/pdf/*.cpp ./src/pdf/
cp $1/src/pdf/*.h ./src/pdf/
cp $1/src/pipe/*.h ./src/pipe/
cp $1/src/pipe/*.cpp ./src/pipe/
cp $1/src/ports/*.h ./src/ports/
cp $1/src/ports/*.cpp ./src/ports/
cp $1/src/svg/*.cpp ./src/svg/
cp $1/src/svg/*.h ./src/svg/
cp $1/src/sfnt/*.cpp ./src/sfnt/
cp $1/src/sfnt/*.h ./src/sfnt/
cp $1/src/text/*.cpp ./src/text/
cp $1/src/utils/mac/SampleApp-Info.plist ./src/utils/mac/
cp $1/src/utils/mac/SampleApp.xib ./src/utils/mac/
cp $1/src/utils/mac/*.cpp ./src/utils/mac/
cp $1/src/utils/mac/*.h ./src/utils/mac/
cp $1/src/utils/mac/*.mm ./src/utils/mac/
cp $1/src/utils/SDL/*.cpp ./src/utils/SDL/
cp $1/src/utils/*.cpp ./src/utils/
cp $1/src/utils/*.h ./src/utils/
cp $1/src/utils/unix/*.c ./src/utils/unix/
cp $1/src/utils/unix/*.cpp ./src/utils/unix/
cp $1/src/utils/win/*.cpp ./src/utils/win/
cp $1/src/views/*.cpp ./src/views/
cp $1/src/views/*.h ./src/views/
cp $1/src/views/views_files.mk ./src/views/
cp $1/src/xml/*.h ./src/xml/
cp $1/src/xml/*.cpp ./src/xml/
cp $1/src/xml/xml_files.mk ./src/xml/

# GLU
cp $1/third_party/glu/gluos.h ./third_party/glu/
cp $1/third_party/glu/libtess/alg-outline ./third_party/glu/libtess/
cp $1/third_party/glu/libtess/*.h ./third_party/glu/libtess/
cp $1/third_party/glu/libtess/*.c ./third_party/glu/libtess/
cp $1/third_party/glu/libtess/GNUmakefile ./third_party/glu/libtess/
cp $1/third_party/glu/libtess/Imakefile ./third_party/glu/libtess/
cp $1/third_party/glu/libtess/README ./third_party/glu/libtess/
cp $1/third_party/glu/LICENSE.txt ./third_party/glu/
cp $1/third_party/glu/README.skia ./third_party/glu/
cp $1/third_party/glu/sk_glu.h ./third_party/glu/

if [ -d $1/.svn ]; then
  rev=$(svn info $1 | awk '/^Revision:/{print $2}')
fi

if [ -n "$rev" ]; then
  version=$rev
  sed -i "" "s/r[0-9][0-9][0-9][0-9]/r$version/" README_MOZILLA
else
  echo "Remember to update README_MOZILLA with the version details."
fi

for file in `ls patches/*.patch`; do
  echo "Applying patch $file"
  patch -p3 < $file
done
