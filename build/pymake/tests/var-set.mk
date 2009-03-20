#T commandline: ['OBASIC=oval']

BASIC = val

TEST = $(TEST)

TEST2 = $(TES
TEST2 += T)

TES T = val

RECVAR = foo
RECVAR += var baz 

IMMVAR := bloo
IMMVAR += $(RECVAR)

BASIC ?= notval

all: BASIC = valall
all: RECVAR += $(BASIC)
all: IMMVAR += $(BASIC)
all: UNSET += more
all: OBASIC += allmore

CHECKLIT = $(NULL) check
all: CHECKLIT += appendliteral

RECVAR = blimey

TESTEMPTY = \
	$(NULL)

all: other
	test "$(TEST2)" = "val"
	test '$(value TEST2)' = '$$(TES T)'
	test "$(RECVAR)" = "blimey valall"
	test "$(IMMVAR)" = "bloo foo var baz  valall"
	test "$(UNSET)" = "more"
	test "$(OBASIC)" = "oval"
	test "$(CHECKLIT)" = " check appendliteral"
	test "$(TESTEMPTY)" = ""
	@echo TEST-PASS

OVAR = oval
OVAR ?= onotval

other: OVAR ?= ooval
other: LATERVAR ?= lateroverride

LATERVAR = olater

other:
	test "$(OVAR)" = "oval"
	test "$(LATERVAR)" = "lateroverride"
