#! gmake

#
# Since everyone seems to need to have their own build configuration
#   system these days, this is yet another makefile to build JavaScript.
#   This makefile conforms to the NSPR20 build rules.  If you have built
#   NSPR20 this will build JS and stick the lib and bin files over in 
#   the dist area created by NSPR (which is different from the dist 
#   expected by the client and also the dist expected by LiveWire, but
#   don't get me started).
#
# I don't currently know enough about what sort of JS-engine the Actra
#   projects are going to expect so I don't know if we need to add
#   to CFLAGS for -DJS_THREADSAFE or -DJSFILE 
#

MOD_DEPTH = ../../nspr20

include $(MOD_DEPTH)/config/config.mk

INCLUDES = -I$(DIST)/include
CFLAGS += -DNSPR20

CSRCS = prmjtime.c \
		  jsapi.c \
		  jsarray.c \
		  jsatom.c \
		  jsbool.c \
		  jscntxt.c \
		  jsdate.c \
		  jsdbgapi.c \
		  jsemit.c \
		  jsfun.c \
		  jsgc.c \
		  jsinterp.c \
		  jsmath.c \
		  jsnum.c \
		  jsobj.c \
		  jsopcode.c \
		  jsparse.c \
		  jsregexp.c \
		  jsscan.c \
		  jsscope.c \
		  jsscript.c \
		  jsstr.c \
		  jslock.c \
		  $(NULL)

HEADERS = jsapi.h \
		  jsarray.h \
		  jsatom.h \
		  jsbool.h \
		  jscntxt.h \
		  jscompat.h \
		  jsconfig.h \
		  jsdate.h \
		  jsdbgapi.h \
		  jsemit.h \
		  jsfun.h \
		  jsgc.h \
		  jsinterp.h \
		  jslock.h \
		  jsmath.h \
		  jsnum.h \
		  jsobj.h \
		  jsopcode.def \
		  jsopcode.h \
		  jsparse.h \
		  jsprvtd.h \
		  jspubtd.h \
		  jsregexp.h \
		  jsscan.h \
		  jsscope.h \
		  jsscript.h \
		  jsstr.h \
		  $(NULL)

ifeq ($(OS_ARCH), WINNT)
EXTRA_LIBS += $(DIST)/lib/libnspr$(MOD_VERSION).lib
EXTRA_LIBS += $(DIST)/lib/libplds$(MOD_VERSION).lib
else
EXTRA_LIBS += -L$(DIST)/lib -lnspr$(MOD_VERSION) -lnplds$(MOD_VERSION)
endif

LIBRARY_NAME	= js
LIBRARY_VERSION	= $(MOD_VERSION)

RELEASE_HEADERS = $(HEADERS)
RELEASE_HEADERS_DEST = $(RELEASE_INCLUDE_DIR)
RELEASE_LIBS	= $(TARGETS)

include $(MOD_DEPTH)/config/rules.mk

export:: $(TARGETS)
	$(INSTALL) -m 444 $(HEADERS) $(MOD_DEPTH)/../dist/public/$(LIBRARY_NAME)
	$(INSTALL) -m 444 $(TARGETS) $(DIST)/lib
	$(INSTALL) -m 444 $(SHARED_LIBRARY) $(DIST)/bin

install:: export

