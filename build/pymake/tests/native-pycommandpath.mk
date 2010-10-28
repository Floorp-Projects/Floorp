#T gmake skip
EXPECTED := some data

# verify that we can load native command modules from
# multiple space-separated directories in PYCOMMANDPATH
CMD = %pycmd writetofile
CMD2 = %pymod writetofile
PYCOMMANDPATH = $(TESTPATH) $(TESTPATH)/subdir

all:
	$(CMD) results $(EXPECTED)
	test "$$(cat results)" = "$(EXPECTED)"
	$(CMD2) results2 $(EXPECTED)
	test "$$(cat results2)" = "$(EXPECTED)"
	@echo TEST-PASS
