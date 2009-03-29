all::
	$(MAKE) -j2 -f $(TESTPATH)/parallel-simple.mk

all:: results
	@echo TEST-PASS
