#!/bin/sh

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
	list=$module_list_dir/mozilla-$p-module-list.txt

	modules=`cat $list | grep -v -e "^#.*$" | grep -v -e "^[ \t]*$"`

	file_list=$outdir/mozilla-$p-file-list.txt
	file_list_devel=$outdir/mozilla-$p-devel-file-list.txt

	tmp_raw=/tmp/raw-list.$$.tmp
	tmp_file_list=/tmp/file-list.$$.tmp
	tmp_file_list_devel=/tmp/file-list-devel.$$.tmp

#	echo "package=$p"
#	echo "modules=$modules"
#	echo "file_list=$file_list"
#	echo "file_list_devel=$file_list_devel"
#	echo "#################"

	rm -f $tmp_raw $file_list $file_list_devel $tmp_file_list $tmp_file_list_devel

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
	cat $tmp_file_list | sort | awk '{ printf("%%{prefix}/lib/mozilla/%s\n" , $0); }' >> $file_list
	cat $tmp_file_list_devel | sort | awk '{ printf("%%{prefix}/lib/mozilla/%s\n" , $0); }' >> $file_list_devel

	# Cleanup
	rm -f $tmp_raw $tmp_file_list $tmp_file_list_devel
done
