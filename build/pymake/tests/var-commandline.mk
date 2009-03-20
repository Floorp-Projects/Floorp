#T commandline: ['TESTVAR=$(MAKEVAL)', 'TESTVAR2:=$(MAKEVAL)']

MAKEVAL=testvalue

all:
	test "$(TESTVAR)" = "testvalue"
	test "$(TESTVAR2)" = ""
	@echo "TEST-PASS"