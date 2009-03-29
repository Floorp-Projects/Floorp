ifdef __WIN32__
PS:=\\#
else
PS:=/
endif

ifneq ($(strip $(MAKEFILE_LIST)),$(NATIVE_TESTPATH)$(PS)include-notfound.mk)
$(error MAKEFILE_LIST incorrect: '$(MAKEFILE_LIST)' (expected '$(NATIVE_TESTPATH)$(PS)include-notfound.mk'))
endif

-include notfound.inc-dummy

ifneq ($(strip $(MAKEFILE_LIST)),$(NATIVE_TESTPATH)$(PS)include-notfound.mk)
$(error MAKEFILE_LIST incorrect: '$(MAKEFILE_LIST)' (expected '$(NATIVE_TESTPATH)$(PS)include-notfound.mk'))
endif

all:
	@echo TEST-PASS

