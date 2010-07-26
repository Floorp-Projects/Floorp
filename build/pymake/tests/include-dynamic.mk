$(shell \
if ! test -f include-dynamic.inc; then \
  echo "TESTVAR = oldval" > include-dynamic.inc; \
  sleep 2; \
  echo "TESTVAR = newval" > include-dynamic.inc.in; \
fi \
)

# before running the 'all' rule, we should be rebuilding include-dynamic.inc,
# because there is a rule to do so

all:
	test $(TESTVAR) = newval
	test "$(MAKE_RESTARTS)" = 1
	@echo TEST-PASS

include-dynamic.inc: include-dynamic.inc.in
	test "$(MAKE_RESTARTS)" = ""
	cp $< $@

include include-dynamic.inc
