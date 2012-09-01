#T gmake skip
#T grep-for: "2f7cdd0b-7277-48c1-beaf-56cb0dbacb24"

ifdef __WIN32__
PS:=;
else
PS:=:
endif

export PATH := $(TESTPATH)/pathdir$(PS)$(PATH)

# This is similar to subprocess-path.mk, except we also check $(shell)
# invocations since they're affected by exported environment variables too,
# but only in submakes!
all:
	$(MAKE) -f $(TESTPATH)/submake-path.makefile2
