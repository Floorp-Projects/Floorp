ifndef TOUCH
TOUCH = touch
endif

all: testfile {testfile2} (testfile3)
	test -f testfile
	test -f {testfile2}
	test -f "(testfile3)"
	@echo TEST-PASS

testfile {testfile2} (testfile3):
	$(TOUCH) "$@"
