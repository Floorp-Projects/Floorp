$(shell touch foo.testfile1 foo.testfile2)

# when a rule has commands and no prerequisites, should it be executed?
# double-colon: yes
# single-colon: no

all: foo.testfile1 foo.testfile2
	test "$$(cat foo.testfile1)" = ""
	test "$$(cat foo.testfile2)" = "remade:foo.testfile2"
	@echo TEST-PASS

foo.testfile1:
	@echo TEST-FAIL

foo.testfile2::
	printf "remade:$@"> $@
