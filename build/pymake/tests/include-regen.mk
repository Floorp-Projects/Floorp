# make should only consider -included makefiles for remaking if they actually exist:

-include notfound.mk

all:
	@echo TEST-PASS

notfound.mk::
	@echo TEST-FAIL
