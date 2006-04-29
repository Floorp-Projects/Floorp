This is the test (and also sample) code for Python in XUL.

* Configure and build Mozilla with --enable-extensions=python,... --enable-tests

  See http://developer.mozilla.org/en/docs/PyXPCOM for any issues regarding
  the building of PyXPCOM itself.  If PyXPCOM builds, this DOM extension 
  should too!

* Start it!

  - From XULRunner, execute:
    % dist/bin/xulrunner xpi-stage/pyxultest/application.ini

  - From Seamonkey (suite) execute:
    % seamonkey -chrome chrome://pyxultest/content

  - From Firebox (browser) execute:
    % firefox ????  (Try the same -chrome as for suite)

Note that on Linux, in all cases you should prefix the command with 
run-mozilla.sh, or any other magic normally necessary...
