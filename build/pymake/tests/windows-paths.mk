all:
	touch file.in
	printf "%s: %s\n\ttrue" '$(CURDIR)/file.out' '$(CURDIR)/file.in' >test.mk
	$(MAKE) -f test.mk $(CURDIR)/file.out
	@echo TEST-PASS
