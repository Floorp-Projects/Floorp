space = $(NULL) $(NULL)
hello$(space)world$(space) = hellovalue

A = aval

VAR = value1\\
VARAWFUL = value1\\#comment
VAR2 = value2
VAR3 = test\$A
VAR4 = value4\\value5

VAR5 = value1\\ \  \
	value2

EPERCENT = \%

all:
	test "$(hello world )" = "hellovalue"
	test "$(VAR)" = "value1\\"
	test '$(VARAWFUL)' = 'value1\'
	test "$(VAR2)" = "value2"
	test "$(VAR3)" = "test\aval"
	test "$(VAR4)" = "value4\\value5"
	test "$(VAR5)" = "value1\\ \ value2"
	test "$(EPERCENT)" = "\%"
	@echo TEST-PASS
