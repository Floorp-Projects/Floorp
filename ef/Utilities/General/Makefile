#!gmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

#######################################################################
# (1) Directory specific info                                         #
#######################################################################

DEPTH = ../..

MODULE_NAME = EF

DIRS = md

CPPSRCS = 	CUtils.cpp					\
		DebugUtils.cpp					\
		FastBitMatrix.cpp				\
		FastBitSet.cpp					\
		FloatUtils.cpp					\
		InterestingEvents.cpp			\
		LogModule.cpp					\
		JavaBytecodes.cpp				\
		Pool.cpp						\
		Tree.cpp						\
		$(NULL)

LOCAL_EXPORTS = CUtils.h					\
		CatchAssert.h					\
		CheckedUnion.h					\
		DebugUtils.h					\
		DoublyLinkedList.h				\
		Exports.h					\
		FastBitMatrix.h					\
		FastBitSet.h					\
		FastHashTable.h					\
		Fifo.h						\
		FloatUtils.h					\
		Fundamentals.h					\
		GraphUtils.h					\
		HashTable.h					\
		InterestingEvents.h			\
		JavaBytecodes.h					\
		LogModule.h					\
		MemoryAccess.h					\
		Multiset.h						\
		NewFastBitMatrix.h				\
		NewFastBitSet.h					\
		Nonspr.h					\
		Pool.h						\
		StringUtils.h					\
		Tree.h						\
		Vector.h					\
		$(NULL)

#######################################################################
# (2) Include "component" configuration information.                  #
#######################################################################

include $(DEPTH)/config/config.mk

#######################################################################
# (3) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

ifeq ($(OS_ARCH),WINNT)
CPPSRCS += CatchAssert.cpp
endif

#######################################################################
# (4) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

include $(DEPTH)/config/rules.mk

#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################



