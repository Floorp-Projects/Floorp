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
# $Id: Makefile,v 1.2 2001/07/12 19:58:23 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

CFLAGS=-Wall -g -I../../../public/ -DJVMP_USE_SHM
all: shmtran.o jvmp_exec host 
jvmp_exec: jvmp_exec.o shmtran.o
	gcc -o jvmp_exec jvmp_exec.o shmtran.o
host: host.o shmtran.o
	 gcc -o host host.o shmtran.o 
clean:
	rm -rf *.o jvmp_exec host core
