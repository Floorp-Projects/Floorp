#T gmake skip

# This test exists to verify that sys.path is adjusted during command
# execution and that delay importing a module will work.

CMD = %pycmd delayloadfn
PYCOMMANDPATH = $(TESTPATH) $(TESTPATH)/subdir

all:
	$(CMD)
	@echo TEST-PASS

