#T commandline: ['-j3']

include $(TESTPATH)/serial-rule-execution.mk

all::
	$(MAKE) -f $(TESTPATH)/parallel-simple.mk

.NOTPARALLEL:
