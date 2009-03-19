# Implicit rules have special instructions to deal with directories, so that a pattern rule which doesn't directly apply
# may still be used.

all: dir/host_test.otest

host_%.otest: %.osource extra.file
	@echo making $@ from $<

test.osource:
	@echo TEST-FAIL should have made dir/test.osource

dir/test.osource:
	@echo TEST-PASS made the correct dependency

extra.file:
	@echo building $@
