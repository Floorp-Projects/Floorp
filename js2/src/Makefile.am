
include $(top_srcdir)/common.mk

INCLUDES = -I$(BOEHM_DIR)

noinst_LIBRARIES = libjs2.a

libjs2_a_LIBADD = $(LIBBOEHM)
libjs2_a_DEPENDENCIES = $(BOEHM_DIR)/gc.a
libjs2_a_SOURCES = \
	algo.h \
	cpucfg.h \
	debugger.cpp \
	debugger.h \
	ds.h \
	exception.cpp \
	exception.h \
	formatter.cpp \
	formatter.h \
	gc_allocator.h \
	gc_container.h \
	hash.cpp \
	hash.h \
	icode_emitter.cpp \
	icodegenerator.cpp \
    icodegenerator.h \
	interpreter.cpp \
	interpreter.h \
	jsmath.cpp \
	jsmath.h \
	jstypes.cpp \
	jstypes.h \
	lexer.cpp \
	lexer.h \
	mem.cpp \
	mem.h \
	nodefactory.h \
	numerics.cpp \
	numerics.h \
	parser.cpp \
	parser.h \
	reader.cpp \
	reader.h \
	stlcfg.h \
	strings.cpp \
	strings.h \
	systemtypes.h \
	token.cpp \
	token.h \
	utilities.cpp \
	utilities.h \
	vmtypes.cpp \
	vmtypes.h \
	world.cpp \
	world.h
