#! gmake

DEPTH = ../..

MODULE = dbm

CSRCS = db.c	   \
	h_bigkey.c \
	h_func.c   \
	h_log2.c   \
	h_page.c   \
	hash.c	   \
	hash_buf.c \
	hsearch.c  \
	memmove.c  \
	mktemp.c   \
	ndbm.c	   \
	snprintf.c \
	strerror.c \
	nsres.c	   \
	$(NULL)

LIBRARY_NAME = dbm

include $(DEPTH)/config/rules.mk

DEFINES += -DMEMMOVE -D__DBINTERFACE_PRIVATE $(SECURITY_FLAG) -DNSPR20=1

INCLUDES += -I../include

ifeq ($(OS_ARCH),AIX)
	OS_LIBS += -lc_r
endif
