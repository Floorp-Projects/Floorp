#!/bin/bash

# Use "build-gtk.sh" or "build-gtk.sh 64" to build a 64-bits tarball for tooltool.
# Use "build-gtk.sh 32" to build a 32-bits tarball for tooltool.

# Mock environments used:
# - 64-bits:
#   https://s3.amazonaws.com/mozilla-releng-mock-archive/67b65e51eb091fba7941a04d249343924a3ee653
#   + libxml2-devel.x86_64 gettext.x86_64 libjpeg-devel.x86_64
# - 32-bits:
#   https://s3.amazonaws.com/mozilla-releng-mock-archive/58d76c6acca148a1aedcbec7fc1b8212e12807b4
#   + libxml2-devel.i686 gettext.i686 libjpeg-devel.i686

set -e

pkg_config_version=0.28
fontconfig_version=2.8.0
libffi_version=3.0.13
glib_version=2.34.3
gdk_pixbuf_version=2.26.5
pixman_version=0.20.2
cairo_version=1.10.2
pango_version=1.30.1
atk_version=2.2.0
gtk__version=3.4.4

pkg_config_url=http://pkgconfig.freedesktop.org/releases/pkg-config-${pkg_config_version}.tar.gz
fontconfig_url=http://www.freedesktop.org/software/fontconfig/release/fontconfig-${fontconfig_version}.tar.gz
libffi_url=ftp://sourceware.org/pub/libffi/libffi-${libffi_version}.tar.gz
glib_url=http://ftp.gnome.org/pub/gnome/sources/glib/${glib_version%.*}/glib-${glib_version}.tar.xz
gdk_pixbuf_url=http://ftp.gnome.org/pub/gnome/sources/gdk-pixbuf/${gdk_pixbuf_version%.*}/gdk-pixbuf-${gdk_pixbuf_version}.tar.xz
pixman_url=http://cairographics.org/releases/pixman-${pixman_version}.tar.gz
cairo_url=http://cairographics.org/releases/cairo-${cairo_version}.tar.gz
pango_url=http://ftp.gnome.org/pub/GNOME/sources/pango/${pango_version%.*}/pango-${pango_version}.tar.xz
atk_url=http://ftp.gnome.org/pub/GNOME/sources/atk/${atk_version%.*}/atk-${atk_version}.tar.xz
gtk__url=http://ftp.gnome.org/pub/gnome/sources/gtk+/${gtk__version%.*}/gtk+-${gtk__version}.tar.xz

cwd=$(pwd)
root_dir=$(mktemp -d)
cd $root_dir

if test -z $TMPDIR; then
  TMPDIR=/tmp/
fi

make_flags=-j12

build() {
	name=$1
	shift
	pkg=$(echo $name | tr '+-' '__')
	version=$(eval echo \$${pkg}_version)
	url=$(eval echo \$${pkg}_url)
	wget -c -P $TMPDIR $url
	tar -axf $TMPDIR/$name-$version.tar.*
	mkdir -p build/$name
	cd build/$name
	eval ../../$name-$version/configure --disable-static $* $configure_args
	make $make_flags
	make install DESTDIR=$root_dir/gtk3
	find $root_dir/gtk3 -name \*.la -delete
	cd ../..
}

case "$1" in
32)
	configure_args='--host=i686-pc-linux --build=i686-pc-linux CC="gcc -m32" CXX="g++ -m32"'
        lib=lib
	;;
*)
	configure_args=
	lib=lib64
	;;
esac

export PKG_CONFIG_LIBDIR=/usr/$lib/pkgconfig:/usr/share/pkgconfig

# The pkg-config version in the mock images is buggy is how it handles
# PKG_CONFIG_SYSROOT_DIR. So we need our own.
build pkg-config

ln -sf /usr/include $root_dir/gtk3/usr/
ln -sf /usr/$lib $root_dir/gtk3/usr/
if [ "$lib" = lib64 ]; then
	ln -s lib $root_dir/gtk3/usr/local/lib64
fi
export PKG_CONFIG_PATH=$root_dir/gtk3/usr/local/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=$root_dir/gtk3
export LD_LIBRARY_PATH=$root_dir/gtk3/usr/local/lib
export PATH=$root_dir/gtk3/usr/local/bin:${PATH}

build fontconfig
build libffi
build glib
build gdk-pixbuf --without-libtiff
build pixman --disable-gtk
build cairo --enable-tee
build pango
build atk
make_flags="$make_flags GLIB_COMPILE_SCHEMAS=glib-compile-schemas"
build gtk+

rm -rf $root_dir/gtk3/usr/local/share/gtk-doc
rm -rf $root_dir/gtk3/usr/local/share/locale

# mock build environment doesn't have fonts in /usr/share/fonts, but
# has some in /usr/share/X11/fonts. Add this directory to the
# fontconfig configuration without changing the gtk3 tooltool package.
cat << EOF > $root_dir/gtk3/usr/local/etc/fonts/local.conf
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <dir>/usr/share/X11/fonts</dir>
</fontconfig>
EOF

cat <<EOF > $root_dir/gtk3/setup.sh
#!/bin/sh

cd \$(dirname \$0)

# pango expects absolute paths in pango.modules, and TOOLTOOL_DIR may vary...
LD_LIBRARY_PATH=./usr/local/lib \
PANGO_SYSCONFDIR=./usr/local/etc \
PANGO_LIBDIR=./usr/local/lib \
./usr/local/bin/pango-querymodules > ./usr/local/etc/pango/pango.modules

# same with gdb-pixbuf and loaders.cache
LD_LIBRARY_PATH=./usr/local/lib \
GDK_PIXBUF_MODULE_FILE=./usr/local/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache \
GDK_PIXBUF_MODULEDIR=./usr/local/lib/gdk-pixbuf-2.0/2.10.0/loaders \
./usr/local/bin/gdk-pixbuf-query-loaders > ./usr/local/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache

# The fontconfig version in the tooltool package has known uses of
# uninitialized memory when creating its cache, and while most users
# will already have an existing cache, running Firefox on automation
# will create it. Combined with valgrind, this generates irrelevant
# errors.
# So create the fontconfig cache beforehand.
./usr/local/bin/fc-cache
EOF

chmod +x $root_dir/gtk3/setup.sh

cd $cwd
tar -C $root_dir -Jcf gtk3.tar.xz gtk3
