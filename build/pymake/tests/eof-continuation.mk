all:
	test '$(TESTVAR)' = 'testval\'
	@echo TEST-PASS

TESTVAR = testval\