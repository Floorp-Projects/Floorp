#!/bin/bash

# hfsplus needs to be rebuilt when changing the clang version used to build it.
# Until bug 1471905 is addressed, increase the following number
# when that happens: 1

set -e
set -x

hfplus_version=540.1.linux3
dirname=diskdev_cmds-${hfplus_version}
make_flags="-j$(nproc)"

root_dir="$1"
if [ -z "$root_dir" -o ! -d "$root_dir" ]; then
  root_dir=$(mktemp -d)
fi
cd $root_dir

if test -z $TMPDIR; then
  TMPDIR=/tmp/
fi

# Build
cd $dirname
# We want to statically link against libcrypto. On CentOS, that requires zlib
# and libdl, because of FIPS functions pulling in more than necessary from
# libcrypto (only SHA1 functions are used), but not on Debian, thus
# --as-needed.
patch -p1 << 'EOF'
--- a/newfs_hfs.tproj/Makefile.lnx
+++ b/newfs_hfs.tproj/Makefile.lnx
@@ -6,3 +6,3 @@
 newfs_hfs: $(OFILES)
-	${CC} ${CFLAGS} ${LDFLAGS} -o newfs_hfs ${OFILES} -lcrypto
+	${CC} ${CFLAGS} ${LDFLAGS} -o newfs_hfs ${OFILES} -Wl,-Bstatic -lcrypto -Wl,-Bdynamic,--as-needed,-lz,-ldl
 
EOF
grep -rl sysctl.h . | xargs sed -i /sysctl.h/d
make $make_flags || exit 1
cd ..

mkdir hfsplus
cp $dirname/newfs_hfs.tproj/newfs_hfs hfsplus/newfs_hfs
## XXX fsck_hfs is unused, but is small and built from the package.
cp $dirname/fsck_hfs.tproj/fsck_hfs hfsplus/fsck_hfs

# Make a package of the built utils
cd $root_dir
tar caf $root_dir/hfsplus.tar.zst hfsplus
