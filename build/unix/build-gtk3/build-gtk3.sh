#!/bin/bash

set -e

fontconfig_version=2.8.0
libffi_version=3.0.13
glib_version=2.34.3
gdk_pixbuf_version=2.26.5
pixman_version=0.20.2
cairo_version=1.10.2
pango_version=1.30.1
atk_version=2.2.0
gtk__version=3.4.4
pulseaudio_version=2.0

fontconfig_url=http://www.freedesktop.org/software/fontconfig/release/fontconfig-${fontconfig_version}.tar.gz
libffi_url=ftp://sourceware.org/pub/libffi/libffi-${libffi_version}.tar.gz
glib_url=http://ftp.gnome.org/pub/gnome/sources/glib/${glib_version%.*}/glib-${glib_version}.tar.xz
gdk_pixbuf_url=http://ftp.gnome.org/pub/gnome/sources/gdk-pixbuf/${gdk_pixbuf_version%.*}/gdk-pixbuf-${gdk_pixbuf_version}.tar.xz
pixman_url=http://cairographics.org/releases/pixman-${pixman_version}.tar.gz
cairo_url=http://cairographics.org/releases/cairo-${cairo_version}.tar.gz
pango_url=http://ftp.gnome.org/pub/GNOME/sources/pango/${pango_version%.*}/pango-${pango_version}.tar.xz
atk_url=http://ftp.gnome.org/pub/GNOME/sources/atk/${atk_version%.*}/atk-${atk_version}.tar.xz
gtk__url=http://ftp.gnome.org/pub/gnome/sources/gtk+/${gtk__version%.*}/gtk+-${gtk__version}.tar.xz
pulseaudio_url=https://freedesktop.org/software/pulseaudio/releases/pulseaudio-${pulseaudio_version}.tar.xz

root_dir=$(mktemp -d)
cd $root_dir

make_flags=-j$(nproc)

# Install a few packages for pulseaudio.
yum install -y libtool-ltdl-devel libtool-ltdl-devel.i686 json-c-devel json-c-devel.i686 libsndfile-devel libsndfile-devel.i686

build() {
	name=$1
	shift
	pkg=$(echo $name | tr '+-' '__')
	version=$(eval echo \$${pkg}_version)
	url=$(eval echo \$${pkg}_url)
	wget -c --progress=dot:mega -P $root_dir $url
	tar -axf $root_dir/$name-$version.tar.*
	mkdir -p build/$name
	cd build/$name
	eval ../../$name-$version/configure --disable-static $* $configure_args --libdir=/usr/local/$lib
	make $make_flags
	make install
	find /usr/local/$lib -name \*.la -delete
	cd ../..
}

for bits in 32 64; do

rm -rf $root_dir/build

case "$bits" in
32)
	configure_args='--host=i686-pc-linux --build=i686-pc-linux CC="gcc -m32" CXX="g++ -m32"'
        lib=lib
	;;
*)
	configure_args=
	lib=lib64
	;;
esac

export PKG_CONFIG_LIBDIR=/usr/local/$lib/pkgconfig:/usr/$lib/pkgconfig:/usr/share/pkgconfig
export LD_LIBRARY_PATH=/usr/local/$lib

build fontconfig
build libffi
build glib
build gdk-pixbuf --without-libtiff --without-libjpeg
build pixman --disable-gtk
build cairo --enable-tee
build pango
build atk
make_flags="$make_flags GLIB_COMPILE_SCHEMAS=glib-compile-schemas"
build gtk+
build pulseaudio --disable-nls

done

rm -rf $root_dir

echo /usr/local/lib > /etc/ld.so.conf.d/local.conf
echo /usr/local/lib64 >> /etc/ld.so.conf.d/local.conf
ldconfig
