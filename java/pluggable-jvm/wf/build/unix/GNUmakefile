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
# $Id: GNUmakefile,v 1.2 2001/07/12 19:57:39 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

include Defs.gmk
#___________________________________________________________________
# sources of Java Plugin DLL
PLUGINCSRCDIR = ../../src/plugin
ifeq ($(USE_SHM), 1)
PLUGINCSRCS =  shmtran.c java_plugin_shm.c
PLUGIN_DLL = libjvmp_shm.so
else
PLUGINCSRCS = jpthreads.c java_plugin.c 
PLUGIN_DLL = libjvmp.so
endif

# source of native methods og WF
PLUGINNATIVESRC=native.c

# native methods of Waterfall itself
PLUGIN_NATIVE_DLL = libJVMPnatives.so

# source of Mozilla-dependent module - should be separeated
MOZEXTSRCDIR = ../../src/plugin/mozilla
MOZEXTCSRC = wfe_mozilla.c wfm6_native.c
MOZEXT_DLL = libwf_moz6.so

PRELOAD_HACK = Helper.libXm.so.4

TESTEXTSRCDIR = ../../src/host
TESTEXTCSRC = test_extension.c
TESTEXT_DLL = libwf_test.so

# source of Netscape4x-dependent module - should be separeated
NCEXTSRCDIR = ../../src/plugin/netscape4
NCEXTCSRC = wfe_netscape4.c shmtran.c
NCEXT_DLL = libwf_netscape4.so

HOSTCPPSRCDIR = ../../src/host
HOSTCPPSRCS = host.cpp
HOSTPROG = plugin_host

VPATH=$(PLUGINCSRCDIR):$(HOSTCPPSRCDIR):$(MOZEXTSRCDIR):$(NCEXTSRCDIR):$(TESTEXTSRCDIR):$(PLUGINCSRCDIR)/unix



PLUGINOBJS=$(PLUGINCSRCS:%.c=%.o) 
PLUGIN_NATIVE_OBJS=$(PLUGINNATIVESRC:%.c=%.o)
HOSTOBJS=$(HOSTCPPSRCS:%.cpp=%.o)
MOZEXTOBJS = $(MOZEXTCSRC:%.c=%.o)
NCEXTOBJS = $(NCEXTCSRC:%.c=%.o)
TESTEXTOBJS=$(TESTEXTCSRC:%.c=%.o)

all: sane
release: all 
	@ cd ../java; $(MAKE) release
	@ rm -rf wfrelease; mkdir wfrelease; mkdir wfrelease/ext
	@ cp $(PRELOAD_HACK) wfrelease
	@ cp wf.jar          wfrelease
	@ cp $(PLUGIN_DLL)   wfrelease
	@ cp $(MOZEXT_DLL)   wfrelease
	@ cp $(PLUGIN_NATIVE_DLL) wfrelease
	@ cp plugin.policy   wfrelease
	@ cp set_paths       wfrelease
	@ cp ext/*.jar       wfrelease/ext
	@ $(TAR) czf wf-bin.tgz wfrelease
	@ echo "Release is wf-bin.tgz"
.SUFFIXES:
.SUFFIXES: .cpp .c .o      
.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<
.c.o:
	$(CC)  $(CFLAGS) -c $< 

ifndef WFJDKHOME 
sane:
	@echo
	@echo "Set WFJDKHOME before"
	@echo
else
sane:   GNUmakefile plugin host_prog ext plugin.policy
endif

natives:	plugin_no_java host_prog
plugin_no_java: plugin_objs plugin_dll plugin_native_dll plugin_exec
plugin:	plugin_no_java java 
plugin_objs:	$(PLUGINOBJS) $(PLUGIN_NATIVE_OBJS)
plugin_dll: $(PLUGIN_DLL)
ext: $(MOZEXT_DLL) $(NCEXT_DLL) $(TESTEXT_DLL) $(PRELOAD_HACK)
$(MOZEXT_DLL): $(MOZEXTOBJS)
	$(LD) $(LIBFLAGS) -o $(MOZEXT_DLL) $(MOZEXTOBJS)
	cp -f $(MOZEXT_DLL) libwf_moz6_g.so
$(PRELOAD_HACK): ../../src/plugin/mozilla/XmHelper.c
	gcc -D_GNU_SOURCE -O2 -fPIC -shared ../../src/plugin/mozilla/XmHelper.c -o $(PRELOAD_HACK)  -ldl
$(TESTEXT_DLL): $(TESTEXTOBJS)
	$(LD) $(LIBFLAGS) -o $(TESTEXT_DLL) $(TESTEXTOBJS)
	cp -f $(TESTEXT_DLL) libwf_test_g.so
ifeq ($(USE_SHM), 1)
$(NCEXT_DLL): $(NCEXTOBJS)
	$(LD) $(LIBFLAGS) -o $(NCEXT_DLL) $(NCEXTOBJS)
	cp -f $(NCEXT_DLL) libwf_netscape4_g.so
else
$(NCEXT_DLL): 
	@echo "Not building Netscape 4 extension if SHM disabled"
endif 
$(PLUGIN_DLL): $(PLUGINOBJS)
	$(LD) $(LIBFLAGS) -o $(PLUGIN_DLL) $(PLUGINOBJS) $(PLUGINLIBS)
plugin_native_dll: $(PLUGIN_NATIVE_DLL)
$(PLUGIN_NATIVE_DLL): $(PLUGIN_NATIVE_OBJS)
	$(LD) $(LIBFLAGS) -o $(PLUGIN_NATIVE_DLL) $(PLUGIN_NATIVE_OBJS) $(JAVALIBS) $(XLIBS)
	cp -f $(PLUGIN_NATIVE_DLL) libJVMPnatives_g.so
java: 
	@cd ../java; $(MAKE) all

host_prog:   $(HOSTOBJS) $(HOSTPROG)

host_objs: $(HOSTOBJS)
$(HOSTPROG): $(HOSTOBJS)
	$(CXX) -o $(HOSTPROG) $(HOSTOBJS) $(HOSTLIBS) 

clean:
	rm -f $(HOSTPROG) $(PLUGINOBJS) $(HOSTOBJS) core *.so *.o $(PRELOAD_HACK) plugin.policy
java_clean:
	@cd ../java; $(MAKE) clean

ifeq ($(USE_SHM), 1)
plugin_exec: jvmp_exec
jvmp_exec: java_plugin_shm_host.o shmtran.o
	$(CC)  $(CFLAGS) -o jvmp_exec java_plugin_shm_host.o  shmtran.o $(PLUGINLIBS)
else
plugin_exec:
endif

plugin.policy: ../../java/plugin.policy
	@$(CP) -f ../../java/plugin.policy .
.PHONY: java clean all host_objs plugin_native_dll plugin_exec


























