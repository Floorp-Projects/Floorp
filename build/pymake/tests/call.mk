test = $0
reverse = $2 $1
twice = $1$1
sideeffect = $(shell echo "called$1:" >>dummyfile)

all:
	test "$(call test)" = "test"
	test "$(call reverse,1,2)" = "2 1"
# expansion happens *before* substitution, thank sanity
	test "$(call twice,$(sideeffect))" = ""
	test `cat dummyfile` = "called:"
	@echo TEST-PASS
