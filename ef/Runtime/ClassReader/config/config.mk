#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

#
# Configuration Makefile
#

machine := $(shell uname)

ifeq ($(machine),SunOS)
  version := $(shell uname -r)
  ifneq ($(word 1, $(subst ., , $(version))), 4)
    arch := sol24
    nspr_arch :=SunOS5.4_DBG.OBJ
  else
    arch := sunos
    nspr_arch :=SunOS5.4_DBG.OBJ
  endif
else 
  ifeq ($(machine), HP-UX)
    nspr_arch :=HP-UXA.09_DBG.OBJ

    version := $(shell uname -r)

    ifneq ($(word 2, $(subst ., , $(version))), 09)
      arch := hpux10
    else
      arch := hpux
    endif
  else
    ifeq ($(machine), AIX)
      arch := aix
      nspr_arch :=AIX4.1_DBG.OBJ
    else
      ifeq ($(machine), OSF1)
        arch := axp
        nspr_arch :=OSF1V3_DBG.OBJ
      else
        ifeq ($(machine), IRIX)
          arch := irix
          nspr_arch :=IRIX5.3_DBG.OBJ
        else
          ifeq ($(machine), Linux)
            arch := linux
            nspr_arch :=LinuxELF2.0_DBG.OBJ
          endif
        endif
      endif
    endif
  endif
endif

OBJHOME := $(DEPTH)/build/obj/$(arch)
BINHOME := $(DEPTH)/build/bin/$(arch)
CFLAGS += -DXP_UNIX -Wno-format -I$(DEPTH)/build/../../../nspr20/pr/include \
  -I$(DEPTH)/build/../../../nspr20/pr/include/md/$(nspr_arch)

