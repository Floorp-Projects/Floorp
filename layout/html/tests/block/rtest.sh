#!/bin/sh

nodots() {
  NODOTS_OUT=$1
  while echo "$NODOTS_OUT" | grep '/\./' > /dev/null
  do
    NODOTS_OUT=`echo $NODOTS_OUT | sed -e"s,/\./,/,"`
  done
  while echo "$NODOTS_OUT" | grep -e '/[^/]*/\.\.' > /dev/null
  do
    NODOTS_OUT=`echo $NODOTS_OUT | sed -e"s,/[^/]*/\.\.,,"`
  done

  echo $NODOTS_OUT
}

dirs="base bugs ../table/core ../table/viewer_tests ../table/bugs ../table/marvin ../table/other ../table/dom"

#extra_dirs="net/HTML_Chars net/W3C net/baron net/boxAcidTest net/glazman net/mozilla"

DEPTH="../../../../.."
TEST_BASE=`dirname $0`
TEST_BASE=`cd $TEST_BASE;pwd`
MOZ_TEST_BASE=$TEST_BASE/$DEPTH
MOZCONF=$HOME/.mozconfig

MOZ_OBJ=""
if test -f $MOZCONF; then
  MOZ_OBJ=`grep -e "^mk_add_options MOZ_OBJDIR=" $MOZCONF | cut -d = -f 2`
fi
if [ -n "$MOZ_OBJ" ]
then
  MOZ_OBJ=`echo $MOZ_OBJ | sed -e"s,@TOPSRCDIR@,$MOZ_TEST_BASE/mozilla,"`
else
  MOZ_OBJ=$MOZ_TEST_BASE/mozilla/
fi

MOZ_TEST_VIEWER="${MOZ_OBJ}dist/bin/mozilla-viewer.sh -- -d 500"
# These are needed by runtests.sh
MOZ_TEST_VIEWER=`nodots $MOZ_TEST_VIEWER`
MOZ_TEST_BASE=`nodots $MOZ_TEST_BASE`
export MOZ_TEST_VIEWER
export MOZ_TEST_BASE

case $1 in
  baseline|verify|clean)
    ;;
  *)
    echo "Usage: $0 baseline|verify|clean"
    exit 1
    ;;
esac

for i in $dirs; do
  cd $TEST_BASE/$i
  echo "Running $1 in $i"
  $TEST_BASE/runtests.sh $1
  cd $TEST_BASE
done
