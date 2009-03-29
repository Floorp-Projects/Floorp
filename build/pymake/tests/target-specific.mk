TESTVAR = anonval

all: target.suffix target.suffix2 dummy host_test.py my.test1 my.test2
	@echo TEST-PASS

target.suffix: TESTVAR = testval

%.suffix:
	test "$(TESTVAR)" = "testval"

%.suffix2: TESTVAR = testval2

%.suffix2:
	test "$(TESTVAR)" = "testval2"

%my: TESTVAR = dummyval

dummy:
	test "$(TESTVAR)" = "dummyval"

%.py: TESTVAR = pyval
host_%.py: TESTVAR = hostval

host_test.py:
	test "$(TESTVAR)" = "hostval"

%.test1 %.test2: TESTVAR = %val

my.test1 my.test2:
	test "$(TESTVAR)" = "%val"
