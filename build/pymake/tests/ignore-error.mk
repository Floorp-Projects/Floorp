all:
	-rm foo
	+-rm bar
	-+rm baz
	@-rm bah
	-@rm humbug
	+-@rm sincere
	+@-rm flattery
	@+-rm will
	@-+rm not
	-+@rm save
	-@+rm you
	@echo TEST-PASS
