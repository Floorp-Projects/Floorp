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

dirs="  base \
        bugs \
        printing \
        ../table/core \
        ../table/viewer_tests \
        ../table/bugs \
        ../table/marvin \
        ../table/other \
        ../table/dom \
        ../table/printing \
        ../formctls/base \
        ../formctls/bugs \
        ../style/bugs \
        ../xbl \
        "

# This doesn't appear to work on Linux right now; needs support for a
# null driver, perhaps?
#extra_dirs="../table/printing"

#extra_dirs="net/HTML_Chars net/W3C net/baron net/boxAcidTest net/glazman net/mozilla"

DEPTH="../../../../.."
TEST_BASE=`dirname $0`
TEST_BASE=`cd $TEST_BASE;pwd`
MOZ_TEST_BASE=$TEST_BASE/$DEPTH
MOZCONF=`$MOZ_TEST_BASE/mozilla/build/autoconf/mozconfig-find $MOZ_TEST_BASE/mozilla`

MOZ_OBJ=""
if test -f $MOZCONF; then
  MOZ_OBJ=`grep -e "^mk_add_options MOZ_OBJDIR=" $MOZCONF | cut -d = -f 2`
fi
if [ -n "$MOZ_OBJ" ]
then
  MOZ_OBJ=`echo $MOZ_OBJ | sed -e"s,@TOPSRCDIR@,$MOZ_TEST_BASE/mozilla,"`
  CFG_GUESS=`$MOZ_TEST_BASE/mozilla/build/autoconf/config.guess`
  MOZ_OBJ=`echo $MOZ_OBJ | sed -e"s,@CONFIG_GUESS@,$CFG_GUESS/,"`
else
  MOZ_OBJ=$MOZ_TEST_BASE/mozilla/
fi

w1=`uname | grep WIN`
if [ "$w1" = "" ]; then
 MOZ_TEST_VIEWER="${MOZ_OBJ}dist/bin/mozilla-viewer.sh -- -d 500"
else
 MOZ_TEST_VIEWER="${MOZ_OBJ}dist/bin/viewer.exe"
fi
# These are needed by runtests.sh
MOZ_TEST_VIEWER=`nodots $MOZ_TEST_VIEWER`
MOZ_TEST_BASE=`nodots $MOZ_TEST_BASE`
export MOZ_TEST_VIEWER
export MOZ_TEST_BASE
XPCOM_DEBUG_BREAK="warn"
export XPCOM_DEBUG_BREAK
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
