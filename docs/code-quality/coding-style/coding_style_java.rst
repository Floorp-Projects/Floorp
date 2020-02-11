=================
Java Coding style
=================

-  We use the `Java Coding
   Style <https://www.oracle.com/technetwork/java/codeconvtoc-136057.html>`__.
   Quick summary:

   -  FirstLetterUpperCase for class names.
   -  camelCase for method and variable names.
   -  One declaration per line:

      .. code-block:: java

         int x, y; // this is BAD!
         int a;    // split it over
         int b;    // two lines

-  Braces should be placed like so (generally, opening braces on same
   line, closing braces on a new line):

   .. code-block:: java

      public void func(int arg) {
          if (arg != 0) {
              while (arg > 0) {
                  arg--;
              }
          } else {
              arg++;
          }
      }

-  Places we differ from the Java coding style:

   -  Start class variable names with 'm' prefix (e.g.
      mSomeClassVariable) and static variables with 's' prefix (e.g.
      sSomeStaticVariable)
   -  ``import`` statements:

      -  Do not use wildcard imports like \`import java.util.*;\`
      -  Organize imports by blocks separated by empty line:
         org.mozilla.*, android.*, com.*, net.*, org.*, then java.\*
         This is basically what Android Studio does by default, except
         that we place org.mozilla.\* at the front - please adjust
         Settings -> Editor -> Code Style -> Java -> Imports
         accordingly.
      -  Within each import block, alphabetize import names with
         uppercase before lowercase. For example, ``com.example.Foo`` is
         before ``com.example.bar``

   -  4-space indents.
   -  Spaces, not tabs.
   -  Don't restrict yourself to 80-character lines. Google's Android
      style guide suggests 100-character lines, which is also the
      default setting in Android Studio. Java code tends to be long
      horizontally, so use appropriate judgement when wrapping. Avoid
      deep indents on wrapping. Note that aligning the wrapped part of a
      line, with some previous part of the line (rather than just using
      a fixed indent), may require shifting the code every time the line
      changes, resulting in spurious whitespace changes.

-  For additional specifics on Firefox for Android, see the `Coding
   Style guide for Firefox on
   Android <https://wiki.mozilla.org/Mobile/Fennec/Android#Coding_Style>`__.
-  The `Android Coding
   Style <https://source.android.com/source/code-style.html>`__ has some
   useful guidelines too.
