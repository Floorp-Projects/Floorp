#T gmake skip
export EXPECTED := some data

CMD = %pycmd writeenvtofile
PYCOMMANDPATH = $(TESTPATH)

all:
	$(CMD) results EXPECTED
	test "$$(cat results)" = "$(EXPECTED)"
	@echo TEST-PASS
