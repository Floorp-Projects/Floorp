#======================================================================
#  FILE: Makefile.am
#  CREATOR: eric 
#  
#  $Id: Makefile.am,v 1.35 2002/11/05 14:59:21 acampi Exp $
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


DESIGNDATA = $(top_srcdir)/design-data
ICALSCRIPTS = $(top_srcdir)/scripts

if WITH_CXX
cxx_lib=libical_cxx.la
cxx_headers=vcomponent.h icalparameter_cxx.h icalproperty_cxx.h icalvalue_cxx.h icptrholder.h
else
cxx_lib=
cxx_headers=
endif

lib_LTLIBRARIES = libical.la $(cxx_lib)

noinst_LTLIBRARIES = libical-static.la
libical_static_la_SOURCES = $(libical_la_SOURCES)
libical_static_la_LDFLAGS = --all-static

CPPFLAGS = -DPACKAGE_DATA_DIR=\""$(datadir)/$(PACKAGE)"\"

# CFLAGS = -g

all: ical.h

INCLUDES =			\
	-I$(top_builddir)	\
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/src	\
	-I$(srcdir)

libical_la_LDFLAGS = -version-info 0:0:0


libical_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	icalarray.c		\
	icalarray.h		\
	icalattach.h		\
	icalattachimpl.h	\
	icalattach.c		\
	icalcomponent.c		\
	icalcomponent.h		\
	icalenums.c		\
	icalenums.h		\
	icalerror.c		\
	icalerror.h		\
	icalmemory.c		\
	icalmemory.h		\
	icalmime.c		\
	icalmime.h		\
	icalparameter.c		\
	icalparameter.h		\
	icalparameterimpl.h	\
	icalparser.c		\
	icalparser.h		\
	icalproperty.c		\
	icalproperty.h		\
	icalrecur.c		\
	icalrecur.h		\
	icalrestriction.h	\
	icaltime.c		\
	icaltime.h		\
	icaltimezone.c		\
	icaltimezone.h		\
	icalduration.h		\
	icalduration.c		\
	icalperiod.h		\
	icalperiod.c		\
	icaltypes.c		\
	icaltypes.h		\
	icalvalue.c		\
	icalvalue.h		\
	icalvalueimpl.h		\
	pvl.c			\
	pvl.h			\
	sspm.c			\
	sspm.h			\
	vsnprintf.c		\
	icallangbind.h		\
	icallangbind.c \
	caldate.c \
	astime.h 

if WITH_CXX
libical_cxx_la_SOURCES =	\
	$(libical_la_SOURCES)	\
	vcomponent.h		\
	vcomponent.cpp		\
	icalproperty_cxx.h	\
	icalproperty_cxx.cpp	\
	icalparameter_cxx.h	\
	icalparameter_cxx.cpp	\
	icalvalue_cxx.h		\
	icalvalue_cxx.cpp       \
	icptrholder.h
endif

libicalincludedir = $(includedir)

libicalinclude_HEADERS = ical.h icalmemory.h icalerror.h $(cxx_headers)

# ORDERING OF HEADERS IS SIGNIFICANT. Don't change this ordering. It
# is required to make the combined header ical.h properly
COMBINEDHEADERS =					\
        $(top_builddir)/config.h                        \
	$(top_builddir)/src/libical/icalversion.h	\
	$(top_srcdir)/src/libical/icaltime.h		\
	$(top_srcdir)/src/libical/icalduration.h	\
	$(top_srcdir)/src/libical/icalperiod.h		\
	$(top_srcdir)/src/libical/icalenums.h		\
	$(top_srcdir)/src/libical/icaltypes.h		\
	$(top_srcdir)/src/libical/icalrecur.h		\
	$(top_srcdir)/src/libical/icalattach.h		\
	icalderivedvalue.h				\
	icalderivedparameter.h				\
	$(top_srcdir)/src/libical/icalvalue.h		\
	$(top_srcdir)/src/libical/icalparameter.h	\
	icalderivedproperty.h				\
	$(top_srcdir)/src/libical/icalproperty.h	\
	$(top_srcdir)/src/libical/pvl.h			\
	$(top_srcdir)/src/libical/icalarray.h		\
	$(top_srcdir)/src/libical/icalcomponent.h	\
	$(top_srcdir)/src/libical/icaltimezone.h	\
	$(top_srcdir)/src/libical/icalparser.h		\
	$(top_srcdir)/src/libical/icalmemory.h		\
	$(top_srcdir)/src/libical/icalerror.h		\
	$(top_srcdir)/src/libical/icalrestriction.h	\
	$(top_srcdir)/src/libical/sspm.h		\
	$(top_srcdir)/src/libical/icalmime.h 		\
	$(top_srcdir)/src/libical/icallangbind.h	

BUILT_COMBINEDHEADERS = 	\
	icalderivedparameter.h	\
	icalderivedproperty.h	\
	icalderivedvalue.h

BUILT_SOURCES =			\
	$(BUILT_COMBINEDHEADERS)\
	icalderivedparameter.c	\
	icalderivedproperty.c	\
	icalrestriction.c	\
	icalderivedvalue.c

ical.h: $(COMBINEDHEADERS)
	echo '#ifdef __cplusplus'         >  ical.h
	echo 'extern "C" {'               >> ical.h
	echo '#endif'                     >> ical.h
	echo '/*'                         >> ical.h
	echo ' $$''Id''$$'                >> ical.h
	echo '*/'                         >> ical.h
	cat $(COMBINEDHEADERS)				\
	| egrep -v "#include.*\"ical"			\
	| egrep -v "#include.*\"config"			\
	| egrep -v "#include.*\"pvl\.h\""		\
	| egrep -v '\$$(Id|Locker): .+\$$'>> ical.h
	echo '#ifdef __cplusplus'         >> ical.h
	echo '};'                         >> ical.h
	echo '#endif'                     >> ical.h


# parameters

PARAMETERDEPS =	\
	$(ICALSCRIPTS)/mkderivedparameters.pl \
	$(DESIGNDATA)/parameters.csv	\
	icalderivedparameter.c.in \
	icalderivedparameter.h.in

icalderivedparameter.h: $(PARAMETERDEPS) 
	$(PERL) -I $(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalderivedparameter.h.in -h $(DESIGNDATA)/parameters.csv > icalderivedparameter.h 

icalderivedparameter.c: $(PARAMETERDEPS) $(BUILT_COMBINEDHEADERS)
	$(PERL) -I $(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalderivedparameter.c.in -c $(DESIGNDATA)/parameters.csv > icalderivedparameter.c

# properties

PROPERTYDEPS =					\
	$(ICALSCRIPTS)/mkderivedproperties.pl	\
	$(DESIGNDATA)/properties.csv		\
	$(DESIGNDATA)/value-types.csv		\
	icalderivedproperty.c.in		\
	icalderivedproperty.h.in


icalderivedproperty.h: $(PROPERTYDEPS)
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedproperties.pl \
		-i $(srcdir)/icalderivedproperty.h.in -h $(DESIGNDATA)/properties.csv\
		${DESIGNDATA}/value-types.csv > icalderivedproperty.h 

icalderivedproperty.c: $(PROPERTYDEPS) $(BUILT_COMBINEDHEADERS)
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedproperties.pl \
		-i $(srcdir)/icalderivedproperty.c.in -c $(DESIGNDATA)/properties.csv \
		${DESIGNDATA}/value-types.csv > icalderivedproperty.c

# restrictions

RESTRICTIONDEPS =				\
	$(ICALSCRIPTS)/mkrestrictiontable.pl	\
	$(DESIGNDATA)/restrictions.csv		\
	icalrestriction.c.in

icalrestriction.c: $(RESTRICTIONDEPS)
	$(PERL) $(ICALSCRIPTS)/mkrestrictiontable.pl  -i $(srcdir)/icalrestriction.c.in \
		$(DESIGNDATA)/restrictions.csv > icalrestriction.c

# values

VALUEDEPS =					\
	$(ICALSCRIPTS)/mkderivedvalues.pl  	\
	$(DESIGNDATA)/value-types.csv		\
	icalderivedvalue.c.in				\
	icalderivedvalue.h.in

icalderivedvalue.h: $(VALUEDEPS)
	$(PERL) -I$(ICALSCRIPTS)  $(ICALSCRIPTS)/mkderivedvalues.pl \
	         -i $(srcdir)/icalderivedvalue.h.in -h $(DESIGNDATA)/value-types.csv > icalderivedvalue.h

icalderivedvalue.c: $(VALUEDEPS) $(BUILT_COMBINEDHEADERS)
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedvalues.pl \
		-i $(srcdir)/icalderivedvalue.c.in -c $(DESIGNDATA)/value-types.csv > icalderivedvalue.c

# c++ needs ical.h auto-generated.
icalparameter_cxx.lo: ical.h
icalproperty_cxx.lo: ical.h
icalvalue_cxx.lo: ical.h
vcomponent.lo: ical.h


# housekeeping
CONFIG_CLEAN_FILES = y.output

CLEANFILES = $(BUILT_SOURCES) $(BUILT_COMBINEDHEADERS)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
	cd $(distdir); rm -f ical.h icalversion.h 

EXTRA_DIST =			\
	icalderivedparameter.c.in \
	icalderivedparameter.h.in \
	icalderivedproperty.c.in \
	icalderivedproperty.h.in \
	icalrestriction.c.in	\
	icalderivedvalue.c.in	\
	icalderivedvalue.h.in	\
	icalversion.h.in

$(srcdir)/icalcomponent.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
icalderivedparameter.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
icalderivedproperty.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
icalderivedvalue.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icalduration.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icallangbind.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icalmime.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icalparameter.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icalparser.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icalproperty.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icalrestriction.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icaltime.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
$(srcdir)/icalvalue.c: icalderivedvalue.h icalderivedproperty.h icalderivedparameter.h
