
include $(top_srcdir)/common.mk

INCLUDES = -I$(top_srcdir)/src/
LIBS = -L$(top_srcdir)/src/ -ljs2

bin_PROGRAMS = parser_test

parser_test_DEPENDENCIES = $(top_srcdir)/src/libjs2.a
parser_test_SOURCES = parser_test.cpp