#!/bin/sh

#
# Given a list of headers, combine them into one, excluding certain lines
#
OUTFILE="/dev/stdout"
COMBINEDHEADERS=""
EXCLUDES=""

while [ $# -gt 0 ]
do
  case $1 in
      -o)  OUTFILE=$2; shift;;
      -e)  EXCLUDES="$EXCLUDES $2"; shift;;
      *)   COMBINEDHEADERS="$COMBINEDHEADERS $1";
  esac
  shift
done

echo '#ifdef __cplusplus'         > $OUTFILE
echo 'extern "C" {'               >> $OUTFILE
echo '#endif'                     >> $OUTFILE
echo '/*'                         >> $OUTFILE
echo " \$Id\$"                    >> $OUTFILE
echo '*/'                         >> $OUTFILE
cat $COMBINEDHEADERS              >> file.temp1
for exclude in $EXCLUDES
do
  cp file.temp1 file.temp2
  egrep -v "$exclude" file.temp2    > file.temp1
done
cat file.temp1 >> $OUTFILE
rm -f file.temp1
rm -f file.temp2
echo '#ifdef __cplusplus'         >> $OUTFILE
echo '};'                         >> $OUTFILE
echo '#endif'                     >> $OUTFILE


