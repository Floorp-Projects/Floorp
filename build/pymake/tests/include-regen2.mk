# make should make makefiles that it has rules for if they are
# included
include test.mk

all:
	test "$(X)" = "1"
	@echo "TEST-PASS"

test.mk:
	@echo "X = 1" > $@
