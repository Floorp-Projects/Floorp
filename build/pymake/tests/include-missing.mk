#T returncode: 2

# If an include file isn't present and doesn't have a rule to remake it, make
# should fail.

include notfound.mk

all:
	@echo TEST-FAIL
