#T commandline: ['-w', 'OVAR=oval']

OVAR=mval

all: vartest run-override
	$(MAKE) -f $(TESTPATH)/override-propagate.mk vartest
	@echo TEST-PASS

SORTED_CLINE := $(sort OVAR=oval TESTPATH=$(TESTPATH) NATIVE_TESTPATH=$(NATIVE_TESTPATH))

vartest:
	@echo MAKELEVEL: '$(MAKELEVEL)'
	test '$(value MAKEFLAGS)' = 'w -- $$(MAKEOVERRIDES)'
	test '$(origin MAKEFLAGS)' = 'file'
	test '$(value MAKEOVERRIDES)' = '$${-*-command-variables-*-}'
	test "$(sort $(MAKEOVERRIDES))" = "$(SORTED_CLINE)"
	test '$(origin MAKEOVERRIDES)' = 'environment'
	test '$(origin -*-command-variables-*-)' = 'automatic'
	test "$(origin OVAR)" = "command line"
	test "$(OVAR)" = "oval"

run-override: MAKEOVERRIDES=
run-override:
	test "$(OVAR)" = "oval"
	$(MAKE) -f $(TESTPATH)/override-propagate.mk otest

otest:
	test '$(value MAKEFLAGS)' = 'w'
	test '$(value MAKEOVERRIDES)' = '$${-*-command-variables-*-}'
	test '$(MAKEOVERRIDES)' = ''
	test '$(origin -*-command-variables-*-)' = 'undefined'
	test "$(OVAR)" = "mval"
