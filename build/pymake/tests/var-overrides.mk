#T commandline: ['CLINEVAR=clineval', 'CLINEVAR2=clineval2']

# this doesn't actually test overrides yet, because they aren't implemented in pymake,
# but testing origins in general is important

MVAR = mval
CLINEVAR = deadbeef

override CLINEVAR2 = mval2

all:
	test "$(origin NOVAR)" = "undefined"
	test "$(CLINEVAR)" = "clineval"
	test "$(origin CLINEVAR)" = "command line"
	test "$(MVAR)" = "mval"
	test "$(origin MVAR)" = "file"
	test "$(@)" = "all"
	test "$(origin @)" = "automatic"
	test "$(origin CLINEVAR2)" = "override"
	test "$(CLINEVAR2)" = "mval2"
	@echo TEST-PASS
