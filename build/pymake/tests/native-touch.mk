TOUCH ?= touch

foo:
	$(TOUCH) bar
	$(TOUCH) baz
	$(MAKE) -f $(TESTPATH)/native-touch.mk baz
	$(TOUCH) -t 198007040802 baz
	$(MAKE) -f $(TESTPATH)/native-touch.mk baz

bar:
	$(TOUCH) $@

baz: bar
	echo TEST-PASS
	$(TOUCH) $@
