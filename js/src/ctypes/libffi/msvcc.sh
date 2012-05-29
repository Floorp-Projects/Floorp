#!/bin/sh

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# GCC-compatible wrapper for cl.exe and ml.exe. Arguments are given in GCC
# format and translated into something sensible for cl or ml.
#

args="-nologo -W3"
md=-MD
cl="cl"
ml="ml"
safeseh="-safeseh"
output=

while [ $# -gt 0 ]
do
  case $1
  in
    -fexceptions)
      # Don't enable exceptions for now.
      #args="$args -EHac"
      shift 1
    ;;
    -m32)
      shift 1
    ;;
    -m64)
      cl="cl"   # "$MSVC/x86_amd64/cl"
      ml="ml64" # "$MSVC/x86_amd64/ml64"
      safeseh=
      shift 1
    ;;
    -O0)
      args="$args -Od"
      shift 1
    ;;
    -O*)
      # If we're optimizing, make sure we explicitly turn on some optimizations
      # that are implicitly disabled by debug symbols (-Zi).
      args="$args $1 -OPT:REF -OPT:ICF -INCREMENTAL:NO"
      shift 1
    ;;
    -g)
      # Enable debug symbol generation.
      args="$args -Zi -DEBUG"
      shift 1
    ;;
    -DFFI_DEBUG)
      # Link against debug CRT and enable runtime error checks.
      args="$args -RTC1"
      defines="$defines $1"
      md=-MDd
      shift 1
    ;;
    -c)
      args="$args -c"
      args="$(echo $args | sed 's%/Fe%/Fo%g')"
      single="-c"
      shift 1
    ;;
    -D*=*)
      name="$(echo $1|sed 's/-D\([^=][^=]*\)=.*/\1/g')"
      value="$(echo $1|sed 's/-D[^=][^=]*=//g')"
      args="$args -D${name}='$value'"
      defines="$defines -D${name}='$value'"
      shift 1
    ;;
    -D*)
      args="$args $1"
      defines="$defines $1"
      shift 1
    ;;
    -I)
      args="$args -I$2"
      includes="$includes -I$2"
      shift 2
    ;;
    -I*)
      args="$args $1"
      includes="$includes $1"
      shift 1
    ;;
    -W|-Wextra)
      # TODO map extra warnings
      shift 1
    ;;
    -Wall)
      # -Wall on MSVC is overzealous, and we already build with -W3. Nothing
      # to do here.
      shift 1
    ;;
    -Werror)
      args="$args -WX"
      shift 1
    ;;
    -W*)
      # TODO map specific warnings
      shift 1
    ;;
    -S)
      args="$args -FAs"
      shift 1
    ;;
    -o)
      outdir="$(dirname $2)"
      base="$(basename $2|sed 's/\.[^.]*//g')"
      if [ -n "$single" ]; then 
        output="-Fo$2"
      else
        output="-Fe$2"
      fi
      if [ -n "$assembly" ]; then
        args="$args $output"
      else
        args="$args $output -Fd$outdir/$base -Fp$outdir/$base -Fa$outdir/$base"
      fi
      shift 2
    ;;
    *.S)
      src=$1
      assembly="true"
      shift 1
    ;;
    *.c)
      args="$args $1"
      shift 1
    ;;
    *)
      # Assume it's an MSVC argument, and pass it through.
      args="$args $1"
      shift 1
    ;;
  esac
done

if [ -n "$assembly" ]; then
    if [ -z "$outdir" ]; then
      outdir="."
    fi
    ppsrc="$outdir/$(basename $src|sed 's/.S$/.asm/g')"
    echo "$cl -nologo -EP $includes $defines $src > $ppsrc"
    "$cl" -nologo -EP $includes $defines $src > $ppsrc || exit $?
    output="$(echo $output | sed 's%/F[dpa][^ ]*%%g')"
    args="-nologo $safeseh $single $output $ppsrc"

    echo "$ml $args"
    eval "\"$ml\" $args"
    result=$?

    # required to fix ml64 broken output?
    #mv *.obj $outdir
else
    args="$md $args"
    echo "$cl $args"
    eval "\"$cl\" $args"
    result=$?
fi

exit $result

