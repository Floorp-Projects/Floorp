# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is The Waterfall Java Plugin Module
# 
# The Initial Developer of the Original Code is Sun Microsystems Inc
# Portions created by Sun Microsystems Inc are Copyright (C) 2001
# All Rights Reserved.
#
# $Id: README.txt,v 1.1 2001/05/09 18:51:32 edburns%acm.org Exp $
#
# 
# Contributor(s): 
#
#   Nikolay N. Igotti <inn@sparc.spb.su>

               Introduction


  This module is an attempt to supply Mozilla with reasonable implementation 
of OJI interfaces. This module uses generic low level library called Waterfall 
as JVM provider. Its JVM integration part is written from scratch, but for 
sake of completness applet viewer from Java Plugin included, so currently
we couldn't publish Waterfall under GPL.
If someone could write reasonable applet viewer, or if I'll 
do it at some moment (currently it's work in progress) - it will be complete 
GPL code. 

                Waterfall

 Anyway, anyone could be able to get Waterfall binaries for Mozilla. 
On Unix platforms (Solaris, Linux) it gives much better performance then 
Java Plugin (as runs in the same process).
Currently Waterfall known to work on following configurations:
  - OS: Solaris 
    ARCH: Sparc
    OS Version: 5.8 (maybe 5.7 with patches)
    JDK: Sun 1.3.1
    VM type: hotspot
    Status: works OK

  - OS: Solaris 
    ARCH: i386
    OS Version: 5.7, 5.8
    JDK: Sun 1.3.1
    VM type: hotspot
    Status: test program  sometimes crashes after AWT loading, 
            seems bug in x86 Solaris or GTK+,
            otherwise works OK

  - OS: Linux
    ARCH: i386
    OS Version: 2.2, glibc 2.2
    JDK: Sun 1.3.1
    VM type: hotspot
    Status: works OK

  - OS: Linux
    ARCH: i386
    OS Version: 2.2, glibc 2.2
    JDK: IBM 1.3
    VM type: classic
    Status: works OK    

  - OS: Windows
    ARCH: i386
    OS Version: NT4.0 (should work on other Win32 systems as far)
    JDK: Sun 1.3
    VM type: hotspot
    Status: works OK
 
Also planned porting Waterfall to Mac and other possible platforms.


      Running
 
 Unix: 
  edit set_env.sh to fit your environment (you need Waterfall headers
  and binaries) and add 
   source set_env.sh 
  to script running Mozilla

 Windows:
  edit set_env.bat to fit your environment (you need Waterfall headers
  and binaries) and run
   set_env.bat
  before running Mozilla
