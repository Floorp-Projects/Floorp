This is the Python/DOM bindings for Mozilla.

These bindings consist of an XPCOM component implementing
nsIScriptRuntime, nsIScriptContext and related interfaces.  See
http://wiki.mozilla.org/Breaking_the_grip_JS_has_on_the_DOM for
a general description of what this means.

The XPCOM component is implemented in C++ and can be found in
the 'src' directory.  This component delegates to a Python implementation
inside a Python 'nsdom' package, which can be found in the nsdom directory.

This directory is built by configuring the Mozilla build process with
the 'python' extenstion enabled - eg, '--enable-extensions=python,default'

The 'test' directory contains all test related code - notably a 'pyxultest'
chrome application.  If you build with --enable-tests, you can run this test
by opening the chrome URL 'chrome://pyxultest/content' (exactly how you do 
that depends on what project you are building). See test/pyxultest/README.txt
for more.
