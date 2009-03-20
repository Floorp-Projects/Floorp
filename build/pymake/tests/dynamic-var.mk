# The *name* of variables can be constructed dynamically.

VARNAME = FOOBAR

$(VARNAME) = foovalue
$(VARNAME)2 = foo2value

$(VARNAME:%BAR=%BAM) = foobam

all:
	test "$(FOOBAR)" = "foovalue"
	test "$(flavor FOOBAZ)" = "undefined"
	test "$(FOOBAR2)" = "bazvalue"
	test "$(FOOBAM)" = "foobam"
	@echo TEST-PASS

VARNAME = FOOBAZ
FOOBAR2 = bazvalue
