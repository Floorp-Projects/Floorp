
include $(top_srcdir)/common.mk

INCLUDES = -I$(JS2_DIR)
LIBS = -L$(JS2_DIR) -ljs2 $(LIBBOEHM)

bin_PROGRAMS = parser_test js2_shell

parser_test_DEPENDENCIES = $(LIBJS2)
parser_test_SOURCES = parser_test.cpp

js2_shell_DEPENDENCIES = $(LIBJS2) $(LIBBOEHM)
js2_shell_LDADD = $(LIBBOEHM)
js2_shell_SOURCES = js2_shell.cpp