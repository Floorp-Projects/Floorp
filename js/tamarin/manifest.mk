INCLUDES += \
  -I$(topsrcdir) \
  -I$(topsrcdir)/MMGC \
  -I$(topsrcdir)/core \
  -I$(topsrcdir)/codegen \
  -I$(topsrcdir)/pcre \
  $(NULL)

$(call RECURSE_DIRS,MMgc core pcre codegen)

ifeq (darwin,$(TARGET_OS))
$(call RECURSE_DIRS,platform/mac)
endif

$(call RECURSE_DIRS,shell)

echo:
	@echo avmplus_CXXFLAGS = $(avmplus_CXXFLAGS)
	@echo avmplus_CXXSRCS = $(avmplus_CXXSRCS)
	@echo avmplus_CXXOBJS = $(avmplus_CXXOBJS)
	@echo avmplus_OBJS = $(avmplus_OBJS)
	@echo avmplus_NAME = $(avmplus_NAME)
