#!/bin/sh
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
#

# The way this thing works:
#
# + A packages file is parsed.  This file contains something 
#   that looks like this (note that spaces are illegal):
#
#   +----------------------------------------------
#   |nspr:nsprpub
#   |core:build,db,xpcom,intl,js,modules/libutil,modules/security/freenav,modules/libpref,modules/libimg,modules/libjar,caps
#   |network:netwerk
#   |layout:htmlparser,gfx,dom,view,widget/timer,widget,layout,webshell,editor,modules/plugin
#   |xpinstall:xpinstall
#   |profile:profile
#   |xptoolkit:xpfe,rdf
#   |cookie:extensions/cookie
#   |wallet:extensions/wallet
#   |mailnews:mailnews
#   +----------------------------------------------
#
# + For each package, a list of modules corresponding to that
#   package is parsed.  Each modules corresponds to a 
#   directory somewhere in a mozilla build tree - usually
#   the toplevel ones (ie, layout, nsprpub, xpcom) but not
#   always (ie, extensions/wallet)
#
# + For each module, print-module-filelist.sh is invoked.
#   The output of that is parsed and converted from the 
#   crazy mozilla install hierarchy to something that
#   makes sense on a linux box.
#
#   For example:  
#
#     bin/components/libraptorhtml.so
#   
#   becomes
#
#     %{prefix}/lib/mozilla/components/libraptorhtml.so
#
# + Also, this script determines which files belong in 
#   a devel package.  For example, "include/*" and "idl/*"

name=generate-package-info.sh

if [ $# -lt 4 ]
then
	echo
	echo "Usage: $name package-list module-list-dir outdir mozdir"
	echo
	exit 1
fi

package_list=$1
module_list_dir=$2
outdir=$3
mozdir=$4

if [ ! -f $package_list ]
then
	echo
	echo "$name: Cant access package file $package_list."
	echo
	exit 1
fi

if [ ! -d $module_list_dir ]
then
	echo
	echo "$name: Cant access module list dir $package_list."
	echo
	exit 1
fi

if [ ! -d $outdir ]
then
	echo
	echo "$name: Cant access outdir $outdir."
	echo
	exit 1
fi

if [ ! -d $mozdir ]
then
	echo
	echo "$name: Cant access mozdir $mozdir."
	echo
	exit 1
fi

rm -rf $outdir/*

packages=`cat $package_list | grep -v -e "^#.*$" | grep -v -e "^[ \t]*$"`

for p in $packages
do
	package=`echo $p | awk -F":" '{ print $1; }'`

	modules=`echo $p | awk -F":" '{ print $2; }' | tr "," " "`
	
	file_list=$outdir/mozilla-$package-file-list.txt
	file_list_devel=$outdir/mozilla-$package-devel-file-list.txt

	tmp_raw=/tmp/raw-list.$$.tmp

	tmp_file_list=/tmp/file-list.$$.tmp
	tmp_file_list_devel=/tmp/file-list-devel.$$.tmp

	tmp_dir_list=/tmp/dir-list.$$.tmp
	tmp_dir_list_devel=/tmp/dir-list-devel.$$.tmp

#	echo "package=$package"
#	echo "modules=$modules"
#	echo "file_list=$file_list"
#	echo "file_list_devel=$file_list_devel"
#	echo "#################"

	rm -f $tmp_raw $file_list $file_list_devel
	rm -f $tmp_file_list $tmp_file_list_devel 
	rm -f $tmp_dir_list $tmp_dir_list_devel

	touch $tmp_raw $file_list $file_list_devel
	touch $tmp_file_list $tmp_file_list_devel
	touch $tmp_dir_list $tmp_dir_list_devel

	print_cmd=$mozdir/build/package/rpm/print-module-filelist.sh

	here=`pwd`

	# Write the raw file list
	for m in $modules
	do
		cd $mozdir/$m
		$print_cmd  >> $tmp_raw
	done

	cd $here

	# Munge the raw list into the file list
	for i in `cat $tmp_raw`
	do
		prefix=`echo $i | awk -F"/" '{ print $1; }'`

		case "$prefix"
		in
			# dirs
			DIR:*)
				dir=`echo $i | cut -b5-`

				case "$dir" 
				in
					include*)
						echo $dir >> $tmp_dir_list_devel
					;;

					*)
						prefix2=`echo $dir | awk -F"/" '{ print $2; }'`

						case "$prefix2"
						in
							# Cut out the "bin/" from these
								res|chrome|defaults)
									echo $dir | cut -b5- >> $tmp_dir_list
								;;
						esac
					;;
				esac
			;;

			##
			## XXX: This one needs to be smarter and catch more devel only
			## stuff.  For example, the gecko viewer and all its resources
			## should go in the devel package.  This would in turn make the
			## regular package smaller.
			##

			# include, idl, lib
			include|idl|lib)
				echo $i >> $tmp_file_list_devel
			;;

			# bin the evil
			bin)
				prefix2=`echo $i | awk -F"/" '{ print $2; }'`

				case "$prefix2"
				in
					# Cut out the "bin/" from these
					components|res|chrome|defaults|netscape.cfg)
						echo $i | cut -b5- >> $tmp_file_list
					;;

					# whatever else in "bin/"
					*)
						# Move special files in "bin/" around
						base=`basename $i`

						case "$base"
						in
						# Mozilla brillantly puts .so files in "bin/" bleh
						*.so)
							echo "lib/$base" >> $tmp_file_list
							;;

						*)
							echo $i >> $tmp_file_list
						;;
						esac
					;;
				esac
			;;

			# whatever else
			*)
				echo $i >> $tmp_file_list
			;;
		esac

	done

	# Spit out sorted file lists
	cat $tmp_dir_list | sort | uniq | awk '{ printf("%%dir %%{prefix}/lib/mozilla/%s\n" , $0); }' >> $file_list

	cat $tmp_file_list | sort | uniq | awk '{ printf("%%{prefix}/lib/mozilla/%s\n" , $0); }' >> $file_list

	cat $tmp_dir_list_devel | sort | uniq | awk '{ printf("%%dir %%{prefix}/lib/mozilla/%s\n" , $0); }' >> $file_list_devel

	cat $tmp_file_list_devel | sort | uniq | awk '{ printf("%%{prefix}/lib/mozilla/%s\n" , $0); }' >> $file_list_devel

	# Cleanup
	rm -f $tmp_raw $tmp_file_list $tmp_file_list_devel $tmp_dir_list $tmp_dir_list_devel
done
