#======================================================================
#  FILE: Makefile.am
#  CREATOR: eric 
#  
#  $Id: Makefile.am,v 1.21 2002/11/05 14:59:22 acampi Exp $
#
#
# (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of either: 
#
#    The LGPL as published by the Free Software Foundation, version
#    2.1, available at: http://www.fsf.org/copyleft/lesser.html
#
#  Or:
#
#    The Mozilla Public License Version 1.0. You may obtain a copy of
#    the License at http://www.mozilla.org/MPL/
#
#  
#
#======================================================================


AM_YFLAGS =-d -v -p ss
AM_LFLAGS = -Pss 
LEX_OUTPUT_ROOT = lex.ss

all: icalss.h

# just to get it built
$(srcdir)/icalgauge.c: icalssyacc.h
$(srcdir)/icalsslexer.c: icalssyacc.h 


if WITH_BDB4
BDB_INCLUDE=-I@BDB_DIR_INCLUDE@ -DWITH_BDB4
BDB_SOURCEFILES=icalbdbset.c icalbdbset.h icalbdbsetimpl.h
BDB_HEADERFILES=$(srcdir)/icalbdbset.h
libicalssinclude_BDBHEADERS=icalbdbset.h icalbdbsetimpl.h
BDB_LIBFILES=@BDB_DIR_LIB@/@BDB_LIB@
else
BDB_INCLUDE=
BDB_SOURCEFILES=
BDB_HEADERFILES=
endif

if WITH_CXX
cxx_lib=libicalss_cxx.la
cxx_headers=icalspanlist_cxx.h
else
cxx_lib=
cxx_headers=
endif

lib_LTLIBRARIES = libicalss.la $(cxx_lib)

INCLUDES =			\
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/src	\
	-I$(top_srcdir)/src/libical	\
	-I$(top_builddir)/libical \
	$(BDB_INCLUDE)

libicalss_la_LDFLAGS = -version-info 0:0:0
libicalss_la_LIBADD = $(BDB_LIBFILES)

if DEV
libicalss_la_DEVSOURCES = icalsslexer.l icalssyacc.y
else
libicalss_la_DEVSOURCES = icalsslexer.c icalssyacc.c
endif

libicalss_la_SOURCES =		\
	icalcalendar.c		\
	icalcalendar.h		\
	icalclassify.c		\
	icalclassify.h		\
	icalcluster.c		\
	icalcluster.h		\
	icalclusterimpl.h	\
	icalgauge.c		\
	icalgauge.h		\
	icalgaugeimpl.h		\
	icaldirset.c		\
	icaldirset.h		\
	icaldirsetimpl.h	\
	icalfileset.c		\
	icalfileset.h		\
	icalfilesetimpl.h	\
	icalset.c		\
	icalset.h		\
	icalssyacc.h		\
	icalspanlist.c		\
	icalspanlist.h		\
	icalmessage.c		\
	icalmessage.h		\
	$(BDB_SOURCEFILES)	\
	$(libicalss_la_DEVSOURCES)

libicalssincludedir = $(includedir)

COMBINEDHEADERS = \
        $(srcdir)/icalgauge.h \
        $(srcdir)/icalset.h \
        $(srcdir)/icalcluster.h \
        $(srcdir)/icalfileset.h \
        $(srcdir)/icaldirset.h \
	$(BDB_HEADERFILES) \
        $(srcdir)/icalcalendar.h  \
        $(srcdir)/icalclassify.h \
	$(srcdir)/icalspanlist.h	\
	$(srcdir)/icalmessage.h


icalss.h: $(COMBINEDHEADERS)
	echo '#ifdef __cplusplus'         >  icalss.h
	echo 'extern "C" {'               >> icalss.h
	echo '#endif'                     >> icalss.h
	echo '/*'                         >> icalss.h
	echo ' $$''Id''$$'                >> icalss.h
	echo '*/'                         >> icalss.h
	cat $(COMBINEDHEADERS)				\
	| egrep -v "#include.*\"ical"			\
	| egrep -v "#include.*\"pvl\.h\""		\
	| egrep -v '\$$(Id|Locker): .+\$$'>> icalss.h
	echo '#ifdef __cplusplus'         >> icalss.h
	echo '};'                         >> icalss.h
	echo '#endif'                     >> icalss.h


if WITH_CXX
libicalss_cxx_la_SOURCES =	\
	$(libicalss_la_SOURCES)	\
	icalspanlist_cxx.h	\
	icalspanlist_cxx.cpp

# c++ needs ical.h auto-generated.
icalspanlist_cxx.lo: icalss.h

endif


libicalssinclude_HEADERS = icalss.h  icalcalendar.h icalclassify.h icalcluster.h icaldirset.h icaldirsetimpl.h icalfileset.h icalfilesetimpl.h icalgauge.h icalgaugeimpl.h icalmessage.h icalset.h icalspanlist.h icalssyacc.h $(libicalssinclude_BDBHEADERS) $(cxx_headers)

CONFIG_CLEAN_FILES = y.output

