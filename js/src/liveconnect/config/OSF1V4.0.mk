#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.

# JDK = /share/builds/components/jdk/1.1.6/OSF1

# INCLUDES   += -I$(JDK)/include/java -I$(JDK)/include/java/alpha

# OTHER_LIBS += -L$(JDK)/lib/alpha -ljava

# XLDFLAGS += -taso

ifndef JDK_VERSION
JDK_VERSION = 1.2.2
endif

# jni.h #error's without this.
CFLAGS += -pthread

JDK = /share/builds/components/jdk/$(JDK_VERSION)/OSF1

# INCLUDES   += -I$(JDK)/include -I$(JDK)/include/solaris
INCLUDES   += -I$(JDK)/include/alpha -I$(JDK)/include

# XXX is this consistent with -pthread above?
OTHER_LIBS += -L$(JDK)/jre/lib/alpha/native_threads
OTHER_LIBS += -L$(JDK)/jre/lib/alpha/classic
OTHER_LIBS += -L$(JDK)/jre/lib/alpha -ljava -ljvm

# Taken from the previous revision - no idea what it does.
XLDFLAGS += -taso
