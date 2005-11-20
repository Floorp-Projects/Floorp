if [[ -z $TEST_HTTP ]]; then
    export TEST_HTTP=test.mozilla.com
fi

if [[ -z $1 ]]; then
    dirs=.
else
    dirs=$@
fi

find $dirs \
    -mindepth 2 -name '*.js' -print | \
    grep -v shell.js | \
    grep -v browser.js | \
    grep -v template.js | \
    sed 's/^\.\///' | \
    while read jsfile
  do
  result=`grep $jsfile spidermonkey-n.tests`
  if [[ -z $result ]]; then
      echo "http://${TEST_HTTP}/tests/mozilla.org/js/js-test-driver-standards.html?test=$jsfile;language=language;javascript"
  fi
done
