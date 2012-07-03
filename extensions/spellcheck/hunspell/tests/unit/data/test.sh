#!/bin/bash
export LC_ALL="C"

function check_valgrind_log () {
if [ "$VALGRIND" != "" ]; then
  if [ -f $TEMPDIR/test.pid* ]; then
    log=`ls $TEMPDIR/test.pid*`
    if ! grep -q 'ERROR SUMMARY: 0 error' $log; then
        echo "Fail in $NAME $1 checking detected by Valgrind"
        echo "$log Valgrind log file moved to $TEMPDIR/badlogs"
        mv $log $TEMPDIR/badlogs
        exit 1
    fi
    if grep -q 'LEAK SUMMARY' $log; then
        echo "Memory leak in $NAME $1 checking detected by Valgrind"
        echo "$log Valgrind log file moved to $TEMPDIR/badlogs"
        mv $log $TEMPDIR/badlogs
        exit 1
    fi    
    rm -f $log
  fi
fi
}

TESTDIR=.
TEMPDIR=$TESTDIR/testSubDir
NAME="$1"
shift

if [ ! -d $TEMPDIR ]; then
  mkdir $TEMPDIR
fi

shopt -s expand_aliases

alias hunspell='../libtool --mode=execute -dlopen ../src/hunspell/.libs/libhunspell*.la ../src/tools/hunspell'
alias analyze='../libtool --mode=execute -dlopen ../src/hunspell/.libs/libhunspell*.la ../src/tools/analyze'

if [ "$VALGRIND" != "" ]; then
  rm -f $TEMPDIR/test.pid*
  if [ ! -d $TEMPDIR/badlogs ]; then
    mkdir $TEMPDIR/badlogs
  fi

  alias hunspell='../libtool --mode=execute -dlopen ../src/hunspell/.libs/libhunspell*.la valgrind --tool=$VALGRIND --leak-check=yes --show-reachable=yes --log-file=$TEMPDIR/test.pid ../src/tools/hunspell'
  alias analyze='../libtool --mode=execute -dlopen ../src/hunspell/.libs/libhunspell*.la valgrind --tool=$VALGRIND --leak-check=yes --show-reachable=yes --log-file=$TEMPDIR/test.pid ../src/tools/analyze'
fi

# Tests good words
if test -f $TESTDIR/$NAME.good; then
    hunspell -l $* -d $TESTDIR/$NAME <$TESTDIR/$NAME.good >$TEMPDIR/$NAME.good
    if test -s $TEMPDIR/$NAME.good; then
        echo "============================================="
        echo "Fail in $NAME.good. Good words recognised as wrong:"
        cat $TEMPDIR/$NAME.good
        rm -f $TEMPDIR/$NAME.good
        exit 1
    fi
    rm -f $TEMPDIR/$NAME.good
fi

check_valgrind_log "good words"

# Tests bad words
if test -f $TESTDIR/$NAME.wrong; then
    hunspell -l $* -d $TESTDIR/$NAME <$TESTDIR/$NAME.wrong >$TEMPDIR/$NAME.wrong
    tr -d '	' <$TESTDIR/$NAME.wrong >$TEMPDIR/$NAME.wrong.detab
    if ! cmp $TEMPDIR/$NAME.wrong $TEMPDIR/$NAME.wrong.detab >/dev/null; then
        echo "============================================="
        echo "Fail in $NAME.wrong. Bad words recognised as good:"
        tr -d '	' <$TESTDIR/$NAME.wrong >$TEMPDIR/$NAME.wrong.detab
        diff $TEMPDIR/$NAME.wrong.detab $TEMPDIR/$NAME.wrong | grep '^<' | sed 's/^..//'
        rm -f $TEMPDIR/$NAME.wrong $TEMPDIR/$NAME.wrong.detab
        exit 1
    fi
    rm -f $TEMPDIR/$NAME.wrong $TEMPDIR/$NAME.wrong.detab
fi

check_valgrind_log "bad words"

# Tests morphological analysis
if test -f $TESTDIR/$NAME.morph; then
    sed 's/	$//' $TESTDIR/$NAME.good >$TEMPDIR/$NAME.good
    analyze $TESTDIR/$NAME.aff $TESTDIR/$NAME.dic $TEMPDIR/$NAME.good >$TEMPDIR/$NAME.morph
    if ! cmp $TEMPDIR/$NAME.morph $TESTDIR/$NAME.morph >/dev/null; then
        echo "============================================="
        echo "Fail in $NAME.morph. Bad analysis?"
        diff $TESTDIR/$NAME.morph $TEMPDIR/$NAME.morph | grep '^<' | sed 's/^..//'
        rm -f $TEMPDIR/$NAME.morph
        exit 1
    fi
    rm -f $TEMPDIR/$NAME.{morph,good}
fi

check_valgrind_log "morphological analysis"

# Tests suggestions
if test -f $TESTDIR/$NAME.sug; then
    hunspell $* -a -d $TESTDIR/$NAME <$TESTDIR/$NAME.wrong | grep '^&' | \
        sed 's/^[^:]*: //' >$TEMPDIR/$NAME.sug 
    if ! cmp $TEMPDIR/$NAME.sug $TESTDIR/$NAME.sug >/dev/null; then
        echo "============================================="
        echo "Fail in $NAME.sug. Bad suggestion?"
        diff $TESTDIR/$NAME.sug $TEMPDIR/$NAME.sug
        rm -f $TEMPDIR/$NAME.sug
        exit 1
    fi
    rm -f $TEMPDIR/$NAME.sug
fi

check_valgrind_log "suggestion"
