#T commandline: ['-w', 'OVAR=oval']

OVAR=mval

all: vartest run-override
	$(MAKE) -f $(TESTPATH)/override-propagate.mk vartest
	@echo TEST-PASS

CLINE := OVAR=oval TESTPATH=$(TESTPATH) NATIVE_TESTPATH=$(NATIVE_TESTPATH)
ifdef __WIN32__
CLINE += __WIN32__=1
endif

SORTED_CLINE := $(subst \,\\,$(sort $(CLINE)))

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
