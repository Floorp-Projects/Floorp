#T commandline: ['-k']
#T returncode: 2
#T grep-for: "TEST-PASS"

all:: t1
	@echo TEST-FAIL "(t1)"

all:: t2
	@echo TEST-PASS

t1:
	@false

t2:
	touch $@

