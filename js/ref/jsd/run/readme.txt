/* jband - 01/13/98 - README... */

In order to actually run this code it is necessary to add some stuff...

0) run the makefile in the directory above to make the executable.

1) compile the stuff in the classes subdir. This supplies a class to launch
the debugger and stubs for the Communicator security classes.

2) add jsd10.jar (from Communicator) to this directory.

3) either create a ifc12.jar from the IFC distributions OR copy the class
tree of the IFC distribution to the classes directory.

4) install a jsdebugger 1.5 frontend. This can be got by installing it
in Communicator and copying jsdeb15.jar to this directory. Also, the 'images' 
and 'sounds' subdirectories need to be copied over.

Inside Netscape the debugger frontend can be found at:
ftp://sweetlou/products/client/jsdebug15/all/1998_01_13_20_26/jsd15su.jar

5) need to install the jre (JavaRuntimeEnvironment) from JavaSoft.