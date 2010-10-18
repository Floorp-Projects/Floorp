ifndef TOUCH
TOUCH = touch
endif

all: testfile
	test -f testfile
	@echo TEST-PASS

testfile:
	$(TOUCH) $@
