#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is lineterm.
#
# The Initial Developer of the Original Code is
# Ramalingam Saravanan.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# rules.mk: Make rules for stand-alone LineTerm only

# CAUTION: Dependency checking is very limited.
# For compilation, the only dependencies are on *.h files in the current
# directory and in the distribution include directory.
# When creating executables, there is additional dependency on
# all *.a files in the distribution object directory.

ifndef LIBRARY
ifdef LIBRARY_NAME
LIBRARY               := lib$(LIBRARY_NAME).$(LIB_SUFFIX)
endif # LIBRARY_NAME
endif # LIBRARY

ifdef PROGRAM
PROGRAM               := $(addprefix $(OBJDIR)/, $(PROGRAM))
endif

ifdef SIMPLE_PROGRAMS
SIMPLE_PROGRAMS       := $(addprefix $(OBJDIR)/, $(SIMPLE_PROGRAMS))
endif

ifdef LIBRARY
LIBRARY               := $(addprefix $(OBJDIR)/, $(LIBRARY))
endif

ifndef OBJS
OBJS = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)
endif

OBJS                  := $(addprefix $(OBJDIR)/, $(OBJS))

ifdef DIRS
LOOP_OVER_DIRS          =                                       \
        @for d in $(DIRS); do                                   \
                if test -f $$d/Makefile; then                   \
                        set -e;                                 \
                        echo "cd $$d; $(MAKE) $@";              \
                        oldDir=`pwd`;                           \
                        cd $$d; $(MAKE) $@; cd $$oldDir;        \
                        set +e;                                 \
                else                                            \
                        echo "Skipping non-directory $$d...";   \
                fi;                                             \
        done
endif

ifndef PROGOBJS
PROGOBJS = $(OBJS)
endif

# Targets
all: export install progs

export: $(EXPORTS)
ifneq (,$(EXPORTS))
	mkdir -p $(topsrcdir)/distrib/include
	+for x in $^; do                                                 \
            rm $(topsrcdir)/distrib/include/$$x;                            \
            echo ln -s `pwd`/$$x $(topsrcdir)/distrib/include/$$x;      \
            ln -s `pwd`/$$x $(topsrcdir)/distrib/include/$$x;      \
         done
endif
	+$(LOOP_OVER_DIRS)

install: $(LIBRARY)
	mkdir -p  $(topsrcdir)/distrib/lib $(topsrcdir)/base/lib
	+$(LOOP_OVER_DIRS)

progs: $(SIMPLE_PROGRAMS)
	mkdir -p  $(topsrcdir)/tests/lib $(topsrcdir)/linetest/lib
	+$(LOOP_OVER_DIRS)

clean:
	-rm $(OBJDIR)/*
	+$(LOOP_OVER_DIRS)

#
# Turn on C++ linking if we have any .cpp files
#
ifdef CPPSRCS
CPP_PROG_LINK = 1
endif

# Create single executable program (with limited dependency checking)
$(PROGRAM): $(PROGOBJS) $(wildcard $(topsrcdir)/distrib/$(OBJDIR)/*.a)
ifeq ($(CPP_PROG_LINK),1)
	$(CCC) -o $@ $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS))
else
	$(CC) -o $@ $(PROGOBJS) $(LDFLAGS) $(LIBS_DIR) $(LIBS)
endif

# Create multiple simple executable programs (with limited dependency checking)
$(SIMPLE_PROGRAMS): $(OBJDIR)/%: $(OBJDIR)/%.o \
                    $(wildcard $(topsrcdir)/distrib/$(OBJDIR)/*.a)
ifeq ($(CPP_PROG_LINK),1)
	$(CCC) -o $@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS))
else
	$(CC) -o $@ $< $(LDFLAGS) $(LIBS_DIR) $(LIBS)
endif

# Create library and export it
$(LIBRARY): $(OBJS)
	$(AR) $(OBJS)
	$(RANLIB) $@
	-rm $(topsrcdir)/distrib/$@
	ln -s `pwd`/$@ $(topsrcdir)/distrib/$@


# Compilation rules (with limited dependency checking)
$(OBJDIR)/%.o: %.c $(wildcard *.h) $(wildcard $(topsrcdir)/distrib/include/*.h)
	$(CC) -o $@ -c $(CFLAGS) $<

$(OBJDIR)/%.o: %.cpp $(wildcard *.h) $(wildcard $(topsrcdir)/distrib/include/*.h)
	$(CCC) -o $@ -c $(CXXFLAGS) $<


# Recognized suffixes
.SUFFIXES:
.SUFFIXES: .a .o .c .cpp .h .pl .class .java .html .mk .in

# Always recognized targets
.PHONY: all clean clobber clobber_all export install

# OS configuration
os_config:
	@echo "OS_ARCH = $(OS_ARCH), OS_CONFIG=$(OS_CONFIG)"
	@echo "OS_RELEASE=$(OS_RELEASE), OS_VERS=$(OS_VERS)"
