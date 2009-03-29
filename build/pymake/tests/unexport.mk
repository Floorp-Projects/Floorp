#T environment: {'ENVVAR': 'envval'}

VAR1 = val1
VAR2 = val2
VAR3 = val3

unexport VAR3
export VAR1 VAR2 VAR3
unexport VAR2 ENVVAR
unexport

all:
	test "$(ENVVAR)" = "envval" # unexport.mk
	$(MAKE) -f $(TESTPATH)/unexport.submk
	@echo TEST-PASS
