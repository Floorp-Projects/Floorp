configure --help works

  $ cd $TESTDIR/../../..

  $ touch $TMP/mozconfig
  $ export MOZCONFIG=$TMP/mozconfig
  $ ./configure --help 2>& 1 | head -n 7
  Adding configure options from */tmp/mozconfig (glob)
  checking for vcs source checkout... hg
  checking for vcs source checkout... hg
  Usage: configure.py [options]
  
  Options: [defaults in brackets after descriptions]
    --help                    print this message
