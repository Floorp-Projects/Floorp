/* jband - 01/16/98 - readme for JSLogger */

INTRO:

JSLogger is a Java Applet/Application which uses the JavaScript Debugger Java
interface (jsd10.jar) to log JavaScript execution. It can be used with
Communicator or with the JavaScript Reference Implementation (with JSD and the
Java VM system running).

JSLogger is useful in cases where the interactive JSDebugger is not approriate.
It also serves as an example of how to use most of the JSD Java API.

DEPENDENCIES:

JSLogger uses a couple of classes from the JavaScript Debugger java applet. At
runtime the classes netscape.jsdebugger.Env and netscape.jsdebugger.Log need to
be findable by the classloader. These classes can be extracted from jsdeb1x.jar.

At compile time it is necessary to have these classes/jars on the classpath:
  - the IFC classes
  - jsd10.jar
  - jsdeb1x.jar

NOTES:

- Many of the features are only going to work right if there is only one 
JS thread.

- JSLogger needs to be run from the file system (as a file: url rather than an
http: or ftp: url). This is just because of a limitation of the Log class.
