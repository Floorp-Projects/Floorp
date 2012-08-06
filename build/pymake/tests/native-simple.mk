ifndef TOUCH
TOUCH = touch
endif

all: testfile {testfile2}
	test -f testfile
	test -f {testfile2}
	@echo TEST-PASS

testfile {testfile2}:
	$(TOUCH) $@
