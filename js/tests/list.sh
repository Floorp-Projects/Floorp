trap 'echo Unexpected error: $?; exit 2' ERR

if [[ -z $TEST_HTTP ]]; then
    export TEST_HTTP=test.mozilla.com
fi

if [[ -z $1 ]]; then
    dirs=.
else
    dirs=$@
fi

currdir=`pwd`
jsdir=`basename $currdir`

find $dirs \
    -mindepth 2 -name '*.js' -print | \
    grep -v shell.js | \
    grep -v browser.js | \
    grep -v template.js | \
    sed 's/^\.\///' | \
    while read jsfile
  do
  result=`grep $jsfile excluded-n.tests`
  if [[ -z $result ]]; then
      result=`echo $jsfile | sed 's/.*js\([0-9]\)_\([0-9]\).*/\1.\2/'`
      case $result in
#	  1.1) version=";version=1.1";;
#	  1.2) version=";version=1.2";;
#	  1.3) version=";version=1.3";;
#	  1.4) version=";version=1.4";;
	  1.5) version=";version=1.5";;
	  1.6) version=";version=1.6";;
	  1.7) version=";version=1.7";;
      esac
      
      echo "http://${TEST_HTTP}/tests/mozilla.org/$jsdir/js-test-driver-standards.html?test=$jsfile;language=type;text/javascript$version"
  fi
done
