# There once was a time when MAKEFLAGS=w without any following spaces would
# cause us to treat w as a target, not a flag. Silly!

MAKEFLAGS=w

all:
	$(MAKE) -f $(TESTPATH)/simple-makeflags.mk subt
	@echo TEST-PASS

subt:
