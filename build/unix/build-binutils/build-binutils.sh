#!/bin/bash

make_flags="-j$(nproc)"

root_dir="$1"

cd $root_dir/binutils-source

patch -p1 <<'EOF'
From 4476cc67e657d6b26cd453c555a611f1ab956660 Mon Sep 17 00:00:00 2001
From: "H.J. Lu" <hjl.tools@gmail.com>
Date: Thu, 30 Aug 2018 09:21:57 -0700
Subject: [PATCH] ld: Lookup section in output with the same name

When there are more than one input sections with the same section name,
SECNAME, linker picks the first one to define __start_SECNAME and
__stop_SECNAME symbols.  When the first input section is removed by
comdat group, we need to check if there is still an output section
with section name SECNAME.

	PR ld/23591
	* ldlang.c (undef_start_stop): Lookup section in output with
	the same name.
---
 ld/ldlang.c | 18 ++++++++++++++++++
 1 file changed, 18 insertions(+)

diff --git a/ld/ldlang.c b/ld/ldlang.c
index 8878ccd..d644b56 100644
--- a/ld/ldlang.c
+++ b/ld/ldlang.c
@@ -6097,6 +6097,24 @@ undef_start_stop (struct bfd_link_hash_entry *h)
       || strcmp (h->u.def.section->name,
 		 h->u.def.section->output_section->name) != 0)
     {
+      asection *sec = bfd_get_section_by_name (link_info.output_bfd,
+					       h->u.def.section->name);
+      if (sec != NULL)
+	{
+	  /* When there are more than one input sections with the same
+	     section name, SECNAME, linker picks the first one to define
+	     __start_SECNAME and __stop_SECNAME symbols.  When the first
+	     input section is removed by comdat group, we need to check
+	     if there is still an output section with section name
+	     SECNAME.  */
+	  asection *i;
+	  for (i = sec->map_head.s; i != NULL; i = i->map_head.s)
+	    if (strcmp (h->u.def.section->name, i->name) == 0)
+	      {
+		h->u.def.section = i;
+		return;
+	      }
+	}
       h->type = bfd_link_hash_undefined;
       h->u.undef.abfd = NULL;
     }
-- 
2.17.1
EOF

cd ..

TARGETS="aarch64-linux-gnu"

# Build target-specific GNU as ; build them first so that the few documentation
# files they install are overwritten by the full binutils build.

for target in $TARGETS; do

  mkdir binutils-$target
  cd binutils-$target

  ../binutils-source/configure --prefix /tools/binutils/ --disable-gold --disable-ld --disable-binutils --disable-gprof --disable-nls --target=$target || exit 1
  make $make_flags || exit 1
  make install $make_flags DESTDIR=$root_dir || exit 1

  cd ..
done

# Build binutils
mkdir binutils-objdir
cd binutils-objdir

# --enable-targets builds extra target support in ld.
# Enabling aarch64 support brings in arm support, so we don't need to specify that too.
../binutils-source/configure --prefix /tools/binutils/ --enable-gold --enable-plugins --disable-nls --enable-targets="$TARGETS" || exit 1
make $make_flags || exit 1
make install $make_flags DESTDIR=$root_dir || exit 1

cd ..

# Make a package of the built binutils
cd $root_dir/tools
tar caf $root_dir/binutils.tar.xz binutils/
