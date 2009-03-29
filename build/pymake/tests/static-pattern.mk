#T returncode: 2

out/host_foo.o: host_%.o: host_%.c out
	cp $< $@
	@echo TEST-FAIL
