# If the dependency graph includes a diamond dependency, we should only remake
# once!

all: depA depB
	cat testfile
	test `cat testfile` = "data";
	@echo TEST-PASS

depA: testfile
depB: testfile

testfile:
	printf "data" >>$@
