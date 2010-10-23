#T gmake skip
EXPECTED := some data

# verify that we can load native command modules from
# multiple directories in PYCOMMANDPATH separated by the native
# path separator
ifdef __WIN32__
PS:=;
else
PS:=:
endif
CMD = %pycmd writetofile
CMD2 = %pymod writetofile
PYCOMMANDPATH = $(TESTPATH)$(PS)$(TESTPATH)/subdir

all:
	$(CMD) results $(EXPECTED)
	test "$$(cat results)" = "$(EXPECTED)"
	$(CMD2) results2 $(EXPECTED)
	test "$$(cat results2)" = "$(EXPECTED)"
	@echo TEST-PASS
