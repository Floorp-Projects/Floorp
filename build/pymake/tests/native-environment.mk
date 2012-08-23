#T gmake skip
export EXPECTED := some data

PYCOMMANDPATH = $(TESTPATH)

all:
	%pycmd writeenvtofile results EXPECTED
	test "$$(cat results)" = "$(EXPECTED)"
	%pycmd writesubprocessenvtofile results EXPECTED
	test "$$(cat results)" = "$(EXPECTED)"
	@echo TEST-PASS
