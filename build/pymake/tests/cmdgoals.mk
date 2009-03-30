default:
	test "$(MAKECMDGOALS)" = ""
	$(MAKE) -f $(TESTPATH)/cmdgoals.mk t1   t2
	@echo TEST-PASS

t1:
	test "$(MAKECMDGOALS)" = "t1 t2"

t2:
