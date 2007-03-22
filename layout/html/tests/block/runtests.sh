#!/bin/sh

# create test file to use first; since we don't know where the tree
# is, and we need full pathnames in the file, we create it on the fly.

case $1 in
 baseline|verify)
    MODE=$1
    ;;

 clean)
    rm -r -f verify baseline  
    exit 0
    ;;

 *)
    echo "Usage: $0 baseline|verify|clean"
    exit 1
    ;;
esac

run_tests() {
    print_flags=

    while [ $# -gt 0 ]; do
        case $1 in
        -m) shift
            mode=$1
            ;;

        -p) print_flags="-Prt 1"
            ;;

        -f) shift
            tests_file=$1
            ;;

        *)  echo "unknown option $1 to run_tests()"
            ;;
        esac
        shift
    done

    if [ -z "$tests_file" ]; then
        echo "no tests file specified to run_tests()"
        exit 1
    fi
    w1=`uname | grep WIN`
    if [ "$mode" = "baseline" ]; then
        echo baseline
        rm -r -f baseline
        mkdir baseline
        echo
        if [ "$w1" = "" ]; then
        echo $MOZ_TEST_VIEWER $print_flags -o baseline/ -f $tests_file
        $MOZ_TEST_VIEWER $print_flags -o baseline/ -f $tests_file
        else
        echo $MOZ_TEST_VIEWER $print_flags -o baseline\\ -f $tests_file
        $MOZ_TEST_VIEWER $print_flags -o baseline\\ -f $tests_file
        fi
    elif [ "$mode" = "verify" ]; then
        rm -r -f verify
        mkdir verify
        echo
        if [ "$w1" = "" ]; then
          echo $MOZ_TEST_VIEWER $print_flags -B 1 -o verify/ -f $tests_file
          $MOZ_TEST_VIEWER $print_flags -B 1 -o verify/ -rd baseline/ -f $tests_file
        else
          echo $MOZ_TEST_VIEWER $print_flags -B 1 -o verify\\ -rd baseline\\ -f $tests_file
          $MOZ_TEST_VIEWER $print_flags -B 1 -o verify\\ -rd baseline\\ -f $tests_file
        fi
    else
        echo "no mode specified to run_tests()"
        exit 1
    fi
}

w1=`uname | grep WIN`
if [ "$w1" = "" ]; then
 TESTS_FILE=/tmp/$$-tests.txt
else
 TESTS_FILE=$TEMP/$$-tests.txt
 TESTS_FILE=`cygpath -w $TESTS_FILE`
 MOZ_TEST_BASE=`cygpath -m $MOZ_TEST_BASE`
fi

cp /dev/null $TESTS_FILE

for FILE in `ls file_list.txt file_list[0-9].txt 2> /dev/null`; do
    egrep -v "^#" < $FILE \
        | sed -e "s@file:///s\(:\||\)@file://$MOZ_TEST_BASE@" \
        >> $TESTS_FILE
done

if [ -s $TESTS_FILE ]; then
    run_tests -m $MODE -f $TESTS_FILE
fi

if [ -f file_list_printing.txt ]; then
    egrep -v "^#" < file_list_printing.txt \
        | sed -e "s@file:///s\(:\||\)@file://$MOZ_TEST_BASE@" \
        > $TESTS_FILE

    if [ -s $TESTS_FILE ]; then
        run_tests -p -m $MODE -f $TESTS_FILE
    fi
fi

