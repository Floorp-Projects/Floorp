#!/bin/csh -x
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
# $Id: set_paths.csh,v 1.2 2001/07/12 19:57:41 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 


setenv WFHOME `pwd`
# Customizable part
setenv JVMTYPE sun
#setenv JVMTYPE ibm
setenv JVM_KIND hotspot
#setenv JVM_KIND classic
setenv XTXMMIX 1
# End of customizable part

switch (`uname -m`)
  case i[3-6]86:
    setenv ARCH i386
    breaksw

  case i86pc:
    setenv ARCH i386
    breaksw

  case sparc*:
    setenv ARCH sparc
    breaksw

  case sun4u:
    setenv ARCH sparc
    breaksw

  default:
    setenv ARCH `uname -m`

endsw

if ($?LD_LIBRARY_PATH) then
# me such a stupid - how to use negation in C shell
else
  setenv LD_LIBRARY_PATH ""
endif

# workaround for bug/feature in JDK - it resolving libjvm.so using 
# dlopen("libjvm.so") - doesn't work if libjvm.so isn't in LD_LIBRARY_PATH
switch ($JVMTYPE)
  case sun:
    setenv LD_LIBRARY_PATH {$WFJDKHOME}/jre/lib/{$ARCH}/{$JVM_KIND}:{$WFJDKHOME}/jre/lib/{$ARCH}:{$LD_LIBRARY_PATH}
    breaksw
  case ibm:
    setenv LD_LIBRARY_PATH $WFJDKHOME/jre/bin/$JVM_KIND:$WFJDKHOME/jre/bin/$ARCH:$LD_LIBRARY_PATH
    breaksw
endsw

if ($XTXMMIX == 1) then
# workaround for mixing of libXt.so and libXm.so in one application, if first
# loaded is libXt.so - as with Mozilla.
  setenv LD_PRELOAD $WFHOME/Helper.libXm.so.4
endif
