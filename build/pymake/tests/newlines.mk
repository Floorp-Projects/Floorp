#T gmake skip

# Test that we handle \\\n properly

all: dep1 dep2 dep3
	cat testfile
	test `cat testfile` = "data";
	test "$$(cat results)" = "$(EXPECTED)";
	@echo TEST-PASS

# Test that something that still needs to go to the shell works
testfile:
	printf "data" \
	  >>$@

dep1: testfile

# Test that something that does not need to go to the shell works
dep2:
	$(echo foo) \
	$(echo bar)

export EXPECTED := some data

CMD = %pycmd writeenvtofile
PYCOMMANDPATH = $(TESTPATH)

dep3:
	$(CMD) \
	results EXPECTED
