all:
	$(MAKE) -f $(TESTPATH)/cmd-stripdotslash.mk ./foo

./foo:
	@echo TEST-PASS
