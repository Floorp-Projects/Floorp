#!/bin/sh

# A hack to make mozilla rpms in place.
here=`pwd`

if [ ! -d ./mozilla ]
then
    printf "\n\nDude, you have to be on the root of the mozilla cvs tree.\n\n"
    exit 1
fi

rpm_place=$here/rpm_on_demand_dir

rm -rf $rpm_place

mkdir -p $rpm_place

mkdir -p $rpm_place/tarball
mkdir -p $rpm_place/home
mkdir -p $rpm_place/topdir
mkdir -p $rpm_place/topdir/BUILD
mkdir -p $rpm_place/topdir/RPMS
mkdir -p $rpm_place/topdir/RPMS/i386
mkdir -p $rpm_place/topdir/RPMS/noarch
mkdir -p $rpm_place/topdir/SOURCES
mkdir -p $rpm_place/topdir/SPECS
mkdir -p $rpm_place/topdir/SRPMS

_top_dir=$rpm_place/topdir

_spec_dir=$_top_dir/SPECS

_sources_dir=$_top_dir/SOURCES

_rpms_dir=$_top_dir/RPMS

_home=$rpm_place/home

_rpm_macros=$_home/.rpmmacros

_tarball_dir=$rpm_place/tarball

#_spec_file=$here/build/package/rpm/mozilla.spec

##
## Setup a phony topdir for the phony rpm macros file
##
echo "%_topdir $_top_dir" >> $_rpm_macros

##
## Make a tarball of the beast
##
cd $_tarball_dir
cvs co mozilla/client.mk
make -f mozilla/client.mk pull_all

tar vzcf mozilla-source.tar.gz mozilla

#XXXX YANK
#cp /tmp/mozilla-source.tar.gz .
#XXXX YANK

tarball=`/bin/ls -1 mozilla*.tar.gz | head -1`

if [ ! -f $tarball ]
then
	printf "\n\nDude, failed to make mozilla tarball.\n\n"
	exit 1
fi

# Put the tarball in the SOURCES dir
mv -f $tarball $_sources_dir

printf "\n\nMozilla tarball = %s\n\n" $_sources_dir/$tarball

# Find the spec file from the rpm 
spec_in_rpm=`tar tzvf $_sources_dir/$tarball |grep -w "mozilla\.spec$" | awk '{ print $6; }'`

printf "\n\nspec_in_rpm=%s\n\n" $spec_in_rpm

# Extract the spec file from the tarball
spec_in_rpm_dir=`echo $spec_in_rpm | awk -F"/" '{ print $1; }'`

printf "\n\nspec_in_rpm_dir=%s\n\n" $spec_in_rpm_dir

tar zvxf $_sources_dir/$tarball $spec_in_rpm

#_spec_file=`/bin/ls -1 $spec_in_rpm_dir | grep "\.spec$" | head -1`

#printf "\n\n_spec_file=%s\n\n" $_spec_file

if [ ! -f $spec_in_rpm ]
then
	printf "\n\nFailed to extract spec file from tarball.\n\n"
	exit 1
fi

# Put the spec file in SPECS
#cp $spec_in_rpm $_spec_dir

#XXXX YANK
cp /tmp/mozilla.spec $_spec_dir
#XXXX YANK

HOME=$_home rpm -ba $_spec_dir/mozilla.spec # > /dev/null 2>&1

if [ $? -eq 0 ]
then
	mkdir -p $rpm_place/RPMS

	cp $_rpms_dir/i386/*.rpm $rpm_place/RPMS/

	last=`/bin/ls -lt1 $rpm_place/RPMS|head -1`

	echo "New RPM written to RPMS/$last"
else
	echo "Failed to build the rpm.  Check the spec file."
fi

echo

cd $rpm_place

# Cleanup
rm -rf $rpm_place/topdir $rpm_place/home
